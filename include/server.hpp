//
// created by xujijun on 2018-03-20
//

#ifndef _XJJ_SERVER_HPP
#define _XJJ_SERVER_HPP

#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include "mutex.hpp"
#include "thread_pool.hpp"

#define BUFFER_SIZE 20
#define MAX_EVENT_COUNT 1024

namespace xjj {

    /*!
     * @brief epoll服务器类 \class
     */
    class Server {
    public:

        /*!
         * @brief 请求类 \class
         */
        class Request {
        private:
            std::string m_body;
        public:
            explicit Request(std::string body);
            const std::string& getBody();
        };

        /*!
         * @brief 响应类 \class
         */
        class Response {
        private:
            int m_sock_fd;

            inline std::string lenToString(int length);
        public:

            /// 套接字文件描述符与Mutex指针哈希表
            static std::unordered_map<int, std::shared_ptr<Mutex>> m_socket_mutex_map;

            explicit Response(int sock_fd);
            void sendResponse(const std::string& body);
        };

        class PacketProcessor {
        private:

            /// 缓冲区长度常量
//            static const int BufferSize;

            /// 读入缓冲区
//            std::unique_ptr<char[]> m_buffer;

            char m_buffer[BUFFER_SIZE]{};

            /// 当前处理数据包
            std::string m_packet;

            /// 当前处理包长
            int32_t m_packet_len;

            /// 包长数据缓冲区
            std::string m_packet_len_buf;

            std::function<void(Request, Response)>& m_business_logic;

            void generatePacket(int data_len, int sock_fd);

            void getPacketLen(int data_len);

            void cutPacketStream();

        public:
            int readBuffer(int sock_fd);

            explicit PacketProcessor(std::function<void(Request, Response)>& business_logic)
                    : m_packet_len(-1),
                      m_business_logic(business_logic) {}
        };

        /*!
         * @brief 构造函数
         * @param [in] business_logic 业务逻辑函数对象
         */
        explicit Server(std::function<void(Request, Response)> business_logic);

        void run();

    private:

        /*!
         * @brief 将文件描述符设置为非阻塞读写模式
         * @param [in] fd 目标文件描述符
         * @return 描述符上原来的模式掩码
         */
        int setNonBlocking(int fd);

        void addFd(int fd, bool enable_one_shot);

        void resetOneShot(int fd);

        void closeSocketConnection(int sock_fd);

        void edgeTriggerEventFunc(int number);

        void initServer();

        void getConfiguration();

        inline int32_t generateTaskId();



        static void printBreakpoint(int pt_id);

        /// 配置文件名
        static const std::string ConfigFileName;

        /// 套接字文件描述符关闭状态码
        static const int CloseSockFdStatusCode;

        /// 重置套接字文件描述符OneShot状态码
        static const int ResetOneShotStatusCode;

        /// epoll文件描述符
        int m_epoll_fd;

        /// epoll事件数组
        epoll_event m_events[MAX_EVENT_COUNT];

        /// 业务逻辑对象
        std::function<void(Request, Response)> m_business_logic;

        /// 线程池对象指针
        std::unique_ptr<ThreadPool> m_thread_pool;

        /// 服务器IP
        std::string m_ip;

        /// 服务器端口
        uint16_t m_port;

        /// 服务器监听套接字文件描述符
        int m_listen_fd;

    };
} // namespace xjj

#endif