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

    /*!
     * @brief 构造函数
     * @param [in] business_logic 业务逻辑函数对象
     */
    Server::Server(std::function<void(const Request&, Response&)> business_logic)
            : m_thread_pool_size(5),
              m_thread_pool_overload(true),
              m_business_logic(std::move(business_logic)),
              m_thread_pool(nullptr) {}

    /*!
     * @brief 析构函数
     */
    Server::~Server() {
        close(m_listen_fd);  // 关闭服务端监听套接字文件描述符
        m_thread_pool -> terminate();  // 终止线程池
    }

    /*!
     * @brief 运行服务器
     */
    void Server::run() {

        printf("Initializing server ...\n");

        initServer();  // 初始化服务器

        printf("\nFinished initialization, going to run.\n");

        while (true)  // 循环等待epoll事件到来
        {
            int ret = epoll_wait(m_epoll_fd, &m_events[0], MAX_EVENT_COUNT, -1);
            if (ret < 0)
            {
                DEBUG_PRINT("epoll failure\n");
                break;
            }

            edgeTriggerEventFunc(ret);
        }
    }

    /*!
     * @brief 将文件描述符设置为非阻塞读写模式
     * @param [in] fd 目标文件描述符
     * @return 文件描述符上原来的模式掩码
     */
    int Server::setNonBlocking(int fd) {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
        return old_option;
    }

    /*!
     * @brief 讲文件描述符fd添加到epoll监听列表中
     * @param [in] fd 目标文件描述符
     * @param [in] enable_one_shot 使用EPOLLONESHOT模式（避免线程竞争读fd）
     */
    void Server::addFd(int fd, bool enable_one_shot) {
        epoll_event event{};
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET;  // 使用ET
        if (enable_one_shot)
        {
            event.events |= EPOLLONESHOT;  // 使用EPOLLONESHOT模式
        }
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event);
        setNonBlocking(fd);
    }

    /*!
     * @brief 对socket文件描述符的EPOLLONESHOT选项进行重置，使后续读事件到来时其他线程可以进行读操作
     * @param [in] fd 目标socket文件描述符
     */
    void Server::resetOneShot(int fd) {
        epoll_event event{};
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }

    /*!
     * @brief epoll事件触发函数
     * @param [in] number 就绪文件描述符数目
     */
    void Server::edgeTriggerEventFunc(int number) {
        for (int i = 0; i < number; i++) {
            int sock_fd = m_events[i].data.fd;
            if (sock_fd == m_listen_fd) {  // 客户端连接事件
                struct sockaddr_in client_address{};
                socklen_t client_addr_length = sizeof(client_address);
                int conn_fd = accept(m_listen_fd, (struct sockaddr *) &client_address, &client_addr_length);
                addFd(conn_fd, true);  // 设置EPOLLONESHOT
            } else if (m_events[i].events & EPOLLIN) {  // 客户端连接可读事件
                DEBUG_PRINT("event trigger once\n");
                m_thread_pool -> addTask(
                        [=] () {
                            DEBUG_PRINT("Going to process packet from sock_fd = %d\n", sock_fd);

                            std::unique_ptr<PacketProcessor>
                                    processor(new PacketProcessor(m_business_logic));

                            // 利用读操作只能单线程执行的特性，初始化对应socket连接的互斥量，以便后续写操作使用
                            if (Response::m_socket_mutex_map.find(sock_fd) == Response::m_socket_mutex_map.end())
                                Response::m_socket_mutex_map[sock_fd] = std::shared_ptr<Mutex>(new Mutex());

                            int ret = processor -> readBuffer(sock_fd);  // 读取处理缓冲区
                            switch (ret) {
                                case CloseSockFdStatusCode: // 处理结果为关闭连接
                                    closeSocketConnection(sock_fd);
                                    break;
                                case ResetOneShotStatusCode: // 处理结果为重置连接，等待可读
                                    resetOneShot(sock_fd);
                                    break;
                                default:
                                    DEBUG_PRINT("something else happened when reading buffer\n");
                            }
                        },
                        generateTaskId()
                );
            } else {
                DEBUG_PRINT("something else happened\n");
            }
        }
    }

    /*!
     * @brief 初始化服务器：包括配置文件加载、服务端监听套接字准备、启动线程池
     */
    void Server::initServer() {
        getConfiguration();  // 配置文件加载

        // 服务端监听套接字准备
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

        m_epoll_fd = epoll_create(5);  // 初始化epoll文件描述符
        assert(m_epoll_fd != -1);
        addFd(m_listen_fd, false);  // 监听套接字不能设置为OneShot！

        m_thread_pool.reset(new ThreadPool(m_thread_pool_size, m_thread_pool_overload));
        m_thread_pool -> start();  // 启动线程池
    }

    /*!
     * @brief 获取配置文件内容
     */
    void Server::getConfiguration() {
        std::ifstream config_fs;
        config_fs.open(ConfigFileName.c_str());  // 打开配置文件

        if (config_fs) {
            std::string exception_msg =
                    "Exception: in function xjj::Server::getConfiguration when getting ";
            std::string config_json, pcs;
            while (config_fs >> pcs) {  // 读取配置文件内容
                config_json.append(pcs);
            }

            // 配置文件为JSON格式，使用rapidJSON进行解析
            rapidjson::Document document;
            document.Parse(config_json.c_str());

            // 对配置文件是否异常进行判断
            if (!document.IsObject())
                throw std::runtime_error(exception_msg + "JSON object from \"./config.json\"");

            if (!document.HasMember("ip") || !document["ip"].IsString())
                throw std::runtime_error(exception_msg + "\"ip\"");

            if (!document.HasMember("port") || !document["port"].IsUint())
                throw std::runtime_error(exception_msg + "\"port\"");

            // 根据配置文件初始化IP和端口
            m_ip = document["ip"].GetString();
            m_port = static_cast<uint16_t>(document["port"].GetUint());

            // 以下为可选配置项
            if (document.HasMember("thread_pool_size")) {
                if (!document["thread_pool_size"].IsUint64()) {
                    throw std::runtime_error(exception_msg + "\"thread_pool_size\"");
                }
                m_thread_pool_size = document["thread_pool_size"].GetUint64();
            }

            if (document.HasMember("thread_pool_overload")) {
                if (!document["thread_pool_overload"].IsBool()) {
                    throw std::runtime_error(exception_msg + "\"thread_pool_overload\"");
                }
                m_thread_pool_overload = document["thread_pool_overload"].GetBool();
            }

        } else {
            throw std::runtime_error("Fail to open \"./config.json\"!");
        }
    }

    /*!
     * @brief 关闭socket连接
     * @param [in] sock_fd 目标socket文件描述符
     */
    void Server::closeSocketConnection(int sock_fd) {
        // 取消对socket文件描述符的epoll事件监听
        epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, sock_fd, nullptr);

        // 删除对应互斥量
        Response::m_socket_mutex_map.erase(sock_fd);

        close(sock_fd);
    }

    /*!
     * @brief 构造函数
     * @param [in] business_logic 用户业务逻辑函数对象
     */
    Server::PacketProcessor::PacketProcessor(std::function<void(const Request&, Response&)>& business_logic)
            : m_packet_len(-1), m_business_logic(business_logic) {}

    /*!
     * @brief 读取并处理缓冲区数据
     * @param [in] sock_fd 欲读取的socket文件描述符
     * @return 操作完成状态码，包括 ResetOneShotStatusCode 和 CloseSockFdStatusCode
     */
    int Server::PacketProcessor::readBuffer(int sock_fd) {
        while (true) {
            bzero(&m_buffer[0], BUFFER_SIZE);  // 清空缓冲区
            int ret = static_cast<int>(recv(sock_fd, &m_buffer[0], BUFFER_SIZE - 1, 0));
            if (ret < 0) { // 出现错误
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    DEBUG_PRINT("Read later\n");
                    return Server::ResetOneShotStatusCode;  // 暂无数据可读，设置EPOLLONESHOT，交由epoll继续监听读事件
                }
                DEBUG_PRINT("Error occur when reading\n");
                return Server::CloseSockFdStatusCode;
            } else if (ret == 0) { // 客户端关闭连接，或者读取到的数据长度为0
                DEBUG_PRINT("TCP peer closed the connection, or 0 bytes data sent.\n");
                return Server::CloseSockFdStatusCode;
            } else {  // 正常读取到数据，进行处理
                DEBUG_PRINT("Got %d bytes of content: %s\n", ret, &m_buffer[0]);
                generatePacket(ret, sock_fd);
            }
        }
    }

    /*!
     * @brief 生成单个请求报文
     * @param [in] data_len 缓冲区中数据长度
     * @param [in] sock_fd 对应socket文件描述符
     */
    void Server::PacketProcessor::generatePacket(int data_len, int sock_fd) {
        if (-1 == m_packet_len) {  // 判断将要处理的报文长度是否已知
            getPacketLen(data_len);  // 报文长度未知，根据字节流解析出报文头，也就是报文长度
        } else {
            printBreakpoint(2);

            // 报文长度已知，讲缓冲区中的数据直接添加到待剪裁报文缓冲区 m_packet 中
            m_packet.append(&m_buffer[0], static_cast<unsigned long>(data_len));
        }

        // 获取待剪裁报文的总长度
        auto packet_real_len = static_cast<int32_t>(m_packet.length());

        DEBUG_PRINT("Before cut: packet_real_len = %d, m_packet = \"%s\"\n",
                packet_real_len, m_packet.c_str());

        if (packet_real_len >= m_packet_len) { // 待剪裁报文长度大于当前处理报文长度，可以进行剪裁

            printBreakpoint(6);

            // 获取剪裁出来的当前处理报文
            std::string valid_packet(m_packet.substr(0, static_cast<unsigned long>(m_packet_len)));

            // 执行业务逻辑
            Request req(valid_packet);
            Response res(sock_fd);
            m_business_logic(req, res);

            // 切分报文边界，获取后续报文的包头
            cutPacketStream();
        }

        DEBUG_PRINT("After cut: m_packet_len = %d, m_packet = \"%s\"\n", m_packet_len, m_packet.c_str());
    }

    /*!
     * @brief 获取单个报文的报文长度（包头）
     * @param [in] data_len 缓冲区中数据长度
     */
    void Server::PacketProcessor::getPacketLen(int data_len) {
        if (0 == m_packet_len_buf.length()) { // 报文头字节数据缓冲区为空，直接读取socket缓冲区中的报文头
            printBreakpoint(0);

            m_packet_len = *reinterpret_cast<const int32_t*>(&m_buffer[0]);
            m_packet.append(&m_buffer[4], static_cast<unsigned long>(data_len - 4));
        } else { // 报文头字节数据缓冲区中已经有部分数据，补全数据后读取出报文头
            printBreakpoint(1);

            int offset = static_cast<int>(4 - m_packet_len_buf.length());
            m_packet_len_buf.append(&m_buffer[0], static_cast<unsigned long>(offset));
            m_packet_len = *reinterpret_cast<const int32_t*>(m_packet_len_buf.c_str());
            m_packet.append(&m_buffer[offset], static_cast<unsigned long>(data_len - offset));
            m_packet_len_buf.clear();  // 清空报文头字节数据缓冲区
        }
    }

    /*!
     * @brief 切分报文边界，获取后续报文的包头
     */
    void Server::PacketProcessor::cutPacketStream() {
        // 获取未剪裁数据长度和当前处理报文长度之差
        int len_diff = static_cast<int>(m_packet.length() - m_packet_len);
        if (len_diff >= 4) { // 未剪裁数据长度和当前处理报文长度之差大于报文头长度，后续报文报文头可知
            printBreakpoint(3);

            int32_t old_len = m_packet_len;
            m_packet_len = *reinterpret_cast<const int32_t*>(m_packet.c_str() + old_len);

            // 根据当前处理报文长度，剪裁出后续报文
            m_packet = m_packet.substr(static_cast<unsigned long>(old_len + 4));
        } else  {  // 未剪裁数据长度和当前处理报文长度之差小于报文头长度，后续报文报文头未知
            printBreakpoint(4);

            if (len_diff > 0) {

                printBreakpoint(5);

                // 根据当前处理报文长度，剪裁出后续报文的部分报文头
                m_packet_len_buf = m_packet.substr(static_cast<unsigned long>(m_packet_len));
            }

            m_packet_len = -1;  // 获取到报文头不完整，后续报文长度未知
            m_packet.clear();  // 清空待剪裁报文缓冲区
        }
    }

    /*!
     * @brief 打印插桩信息，采用 printDebugInfo 实现
     * @param [in] pt_id 插桩编号
     */
    void Server::printBreakpoint(int pt_id) {
        std::string msg = "breakpoint ";
        msg.append(std::to_string(pt_id));
        msg.append(1, '\n');
        DEBUG_PRINT(msg.c_str());
    }

    /*!
     * @brief 任务Id生成，用于标识即将加入线程池任务队列的任务
     * @return 任务Id
     */
    int32_t Server::generateTaskId() {
        return -1;
    }

    /*!
     * @brief 打印调试信息（使用宏 _XJJ_DEBUG 开启调试模式），采用 printf 实现
     * @param [in] format 格式字符指针常量
     * @param [in] ... 指定格式的可变长参数列表
     */
    void Server::printDebugInfo(const char *format, ...) {
        va_list arg_ptr;

        va_start(arg_ptr, format);  // 获取可变参数列表
        fflush(stdout);  // 刷新输出缓冲区
        vfprintf(stderr, format, arg_ptr);  // 输出调试信息
        va_end(arg_ptr);  // 可变参数列表结束
    }

    /*!
     * @brief 构造函数
     * @param [in] body 初始化请求体参数
     */
    Server::Request::Request(const std::string& body)
            : m_body(body) {}

    /*!
     * @brief 获取请求体
     * @return 请求体
     */
    const std::string &Server::Request::getBody() const {
        return m_body;
    }

    /*!
     * @brief 构造函数
     * @param [in] sock_fd 初始化响应的套接字文件描述符
     */
    Server::Response::Response(int sock_fd)
            : m_sock_fd(sock_fd) {}

    /*!
     * @brief 将报文体的长度转换成字符表示，用于做为响应报文头
     * @param [in] length 报文体长度
     * @return 转换结果
     */
    std::string Server::Response::lenToString(int length) {
        std::string res(4, '\0');
        for (int i = 0; i < 4; i++) {
            res[i] = static_cast<char>(length >> (i * 8));
        }
        return res;
    }

    /// 套接字文件描述符与Mutex指针哈希表
    std::unordered_map<int, std::shared_ptr<Mutex>> Server::Response::m_socket_mutex_map;

    /*!
     * @brief 发送响应报文
     * @param [in] body 响应报文体
     */
    void Server::Response::sendResponse(const std::string &body) {
        std::string packet(  // 初始化响应报文，添加报文头
                std::move(  // 采用移动构造，减少内存拷贝开销
                        lenToString(static_cast<int>(body.length()))
                )
        );

        packet.append(body); // 添加报文体

        DEBUG_PRINT("Going to send: %s", packet.c_str());

        // 使用连接fd匹配互斥量加锁，防止多线程竞争写同个fd
        AutoLockMutex autoLockMutex(m_socket_mutex_map[m_sock_fd].get());
        send(m_sock_fd, packet.c_str(), packet.length(), 0);
    }
} // namespace xjj