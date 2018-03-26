//
// created by xujijun on 2018-03-20
//

#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <rapidjson/document.h>
#include "server.hpp"

namespace xjj {

    /// 配置文件名
    const std::string Server::ConfigFileName = "./config.json";

    /// 套接字文件描述符关闭状态码
    const int Server::CloseSockFdStatusCode = -1;

    /// 重置套接字文件描述符OneShot状态码
    const int Server::ResetOneShotStatusCode = -2;

    Server::Server(std::function<void(Request, Response)> business_logic)
            : m_business_logic(std::move(business_logic)),
              m_thread_pool(new ThreadPool()) {}

    void Server::run() {
        initServer();

        while (true)
        {
            int ret = epoll_wait(m_epoll_fd, &m_events[0], MAX_EVENT_COUNT, -1);
            if (ret < 0)
            {
                printf("epoll failure\n");
                break;
            }

            edgeTriggerEventFunc(ret);
        }

        close(m_listen_fd);

        m_thread_pool -> terminate();
    }

    int Server::setNonBlocking(int fd) {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
        return old_option;
    }

    void Server::addFd(int fd, bool enable_one_shot) {
        epoll_event event{};
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET;  // 使用ET
        if (enable_one_shot)
        {
            event.events |= EPOLLONESHOT;
        }
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event);
        setNonBlocking(fd);
    }

    void Server::resetOneShot(int fd) {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }

    void Server::edgeTriggerEventFunc(int number) {
        for (int i = 0; i < number; i++) {
            int sock_fd = m_events[i].data.fd;
            if (sock_fd == m_listen_fd) {
                struct sockaddr_in client_address{};
                socklen_t client_addr_length = sizeof(client_address);
                int conn_fd = accept(m_listen_fd, (struct sockaddr *) &client_address, &client_addr_length);
                addFd(conn_fd, true);  // 设置EPOLLONESHOT
            } else if (m_events[i].events & EPOLLIN) {
                printf("event trigger once\n");
                m_thread_pool -> addTask(
                        [=] () {
                            printf("Going to process packet from sock_fd = %d\n", sock_fd);

                            std::unique_ptr<PacketProcessor>
                                    processor(new PacketProcessor(m_business_logic));

                            // 利用读操作只能单线程执行的特性，初始化对应socket连接的互斥量，以便后续写操作使用
                            if (Response::m_socket_mutex_map.find(sock_fd) == Response::m_socket_mutex_map.end())
                                Response::m_socket_mutex_map[sock_fd] = std::shared_ptr<Mutex>(new Mutex());

                            int ret = processor -> readBuffer(sock_fd);
                            switch (ret) {
                                case CloseSockFdStatusCode:
                                    closeSocketConnection(sock_fd);
                                    break;
                                case ResetOneShotStatusCode:
                                    resetOneShot(sock_fd);
                                    break;
                                default:
                                    printf("something else happened when reading buffer\n");
                            }
                        },
                        generateTaskId()
                );
            } else {
                printf("something else happened\n");
            }
        }
    }

    void Server::initServer() {
        getConfiguration();
        int ret = 0;
        struct sockaddr_in address{};
        bzero(&address, sizeof(address));
        address.sin_family = AF_INET;
        inet_pton(AF_INET, m_ip.c_str(), &address.sin_addr);
        address.sin_port = htons(m_port);

        m_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
        assert(m_listen_fd >= 0);

        int opt = 1;
        setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        ret = bind(m_listen_fd, (struct sockaddr *)&address, sizeof(address));
        assert(ret != -1);

        ret = listen(m_listen_fd, 5);
        assert(ret != -1);

        m_epoll_fd = epoll_create(5);
        assert(m_epoll_fd != -1);
        addFd(m_listen_fd, false);  // 监听套接字不能设置为OneShot！

        m_thread_pool -> start();
    }

    void Server::getConfiguration() {
        std::ifstream config_fs;
        config_fs.open(ConfigFileName.c_str());

        if (config_fs) {
            std::string exception_msg =
                    "Exception: in function xjj::Server::getConfiguration when getting ";
            std::string config_json, pcs;
            while (config_fs >> pcs) {
                config_json.append(pcs);
            }

            rapidjson::Document document;
            document.Parse(config_json.c_str());

            if (!document.IsObject())
                throw std::runtime_error(exception_msg + "JSON object from \"./config.json\"");

            if (!document.HasMember("ip") || !document["ip"].IsString())
                throw std::runtime_error(exception_msg + "\"ip\"");

            if (!document.HasMember("port") || !document["port"].IsUint())
                throw std::runtime_error(exception_msg + "\"port\"");

            m_ip = document["ip"].GetString();
            m_port = static_cast<uint16_t>(document["port"].GetUint());

        } else {
            throw std::runtime_error("Fail to open \"./config.json\"!");
        }
    }

    void Server::closeSocketConnection(int sock_fd) {
        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, sock_fd, nullptr);

        // 删除对应互斥量
        Response::m_socket_mutex_map.erase(sock_fd);

        close(sock_fd);
    }

    int Server::PacketProcessor::readBuffer(int sock_fd) {
        while (true) {
            bzero(&m_buffer[0], BUFFER_SIZE);
            int ret = static_cast<int>(recv(sock_fd, &m_buffer[0], BUFFER_SIZE - 1, 0));
            if (ret < 0) {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    printf("Read later\n");
                    return Server::ResetOneShotStatusCode;  // 暂无数据可读，设置EPOLLONESHOT，交由epoll继续监听读事件
                }
                printf("Error occur when reading\n");
                return Server::CloseSockFdStatusCode;
            } else if (ret == 0) {
                printf("TCP peer closed the connection, or 0 bytes data sent.\n");
                return Server::CloseSockFdStatusCode;
            } else {
                printf("Got %d bytes of content: %s\n", ret, &m_buffer[0]);
                generatePacket(ret, sock_fd);
            }
        }
    }

    void Server::PacketProcessor::generatePacket(int data_len, int sock_fd) {
        if (-1 == m_packet_len) {
            getPacketLen(data_len);
        } else {
            printBreakpoint(2);

            m_packet.append(&m_buffer[0], static_cast<unsigned long>(data_len));
        }
        auto packet_real_len = static_cast<int32_t>(m_packet.length());

        std::cout << "Before cut: packet_real_len = " << packet_real_len
                  << ", m_packet = \"" << m_packet << '\"' << std::endl;

        if (packet_real_len >= m_packet_len) {

            printBreakpoint(6);

            std::string valid_packet(m_packet.substr(0, static_cast<unsigned long>(m_packet_len)));

            // 执行业务逻辑
            m_business_logic(Request(valid_packet), Response(sock_fd));

            cutPacketStream();
        }
        std::cout << "After cut: m_packet_len = " << m_packet_len << ", m_packet = \"" << m_packet << '\"' << std::endl;
    }

    void Server::PacketProcessor::getPacketLen(int data_len) {
        if (0 == m_packet_len_buf.length()) {
            printBreakpoint(0);

            m_packet_len = *reinterpret_cast<const int32_t*>(&m_buffer[0]);
            m_packet.append(&m_buffer[4], static_cast<unsigned long>(data_len - 4));
        } else {
            printBreakpoint(1);

            int offset = static_cast<int>(4 - m_packet_len_buf.length());
            m_packet_len_buf.append(&m_buffer[0], static_cast<unsigned long>(offset));
            m_packet_len = *reinterpret_cast<const int32_t*>(m_packet_len_buf.c_str());
            m_packet.append(&m_buffer[offset], static_cast<unsigned long>(data_len - offset));
            m_packet_len_buf.clear();
        }
    }

    void Server::PacketProcessor::cutPacketStream() {
        int len_diff = static_cast<int>(m_packet.length() - m_packet_len);
        if (len_diff >= 4) {
            printBreakpoint(3);

            int32_t old_len = m_packet_len;
            m_packet_len = *reinterpret_cast<const int32_t*>(m_packet.c_str() + old_len);
            m_packet = m_packet.substr(static_cast<unsigned long>(old_len + 4));
        } else  {
            printBreakpoint(4);

            if (len_diff > 0) {

                printBreakpoint(5);

                m_packet_len_buf = m_packet.substr(static_cast<unsigned long>(m_packet_len));
            }

            m_packet_len = -1;
            m_packet.clear();
        }
    }

    void Server::printBreakpoint(int pt_id) {
        std::cout << std::endl;
        std::cout << "breakpoint " << pt_id << std::endl;
        std::cout << std::endl;
    }

    int32_t Server::generateTaskId() {
        return -1;
    }

    Server::Request::Request(std::string body)
            : m_body(std::move(body)) {}

    const std::string &Server::Request::getBody() {
        return m_body;
    }

    Server::Response::Response(int sock_fd)
            : m_sock_fd(sock_fd) {}

    std::string Server::Response::lenToString(int length) {
        std::string res(4, '\0');
        for (int i = 0; i < 4; i++) {
            res[i] = static_cast<char>(length >> (i * 8));
        }
        return res;
    }

    /// 套接字文件描述符与Mutex指针哈希表
    std::unordered_map<int, std::shared_ptr<Mutex>> Server::Response::m_socket_mutex_map;

    void Server::Response::sendResponse(const std::string &body) {
        std::string packet(
                std::move(
                        lenToString(static_cast<int>(body.length()))
                )
        );

        packet.append(body);

        std::cout << "Going to send: " << packet << std::endl;

        // 使用连接fd匹配互斥量加锁，防止多线程竞争写同个fd
        AutoLockMutex autoLockMutex(m_socket_mutex_map[m_sock_fd].get());
        send(m_sock_fd, packet.c_str(), packet.length(), 0);
    }
} // namespace xjj