//
// created by xujijun on 2018-03-20
//

/// 调试模式选项，为1时将打印调试信息
#define _XJJ_DEBUG 0

#if _XJJ_DEBUG
#define DEBUG_PRINT printDebugInfo
#else
#define DEBUG_PRINT
#endif

#ifndef _XJJ_SERVER_HPP
#define _XJJ_SERVER_HPP

#include <cstdarg>
#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include "mutex.hpp"
#include "thread_pool.hpp"

/// 缓冲区大小
#define BUFFER_SIZE 20

/// epoll监听事件数目上限建议值
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

            /// 请求体
            std::string m_body;

        public:

            /*!
             * @brief 构造函数
             * @param [in] body 初始化请求体参数
             */
            explicit Request(const std::string& body);

            /*!
             * @brief 获取请求体
             * @return 请求体
             */
            const std::string& getBody() const;
        };

        /*!
         * @brief 响应类 \class
         */
        class Response {
        private:

            /// 响应的套接字文件描述符
            int m_sock_fd;

            /*!
             * @brief 将报文体的长度转换成字符表示，用于做为响应报文头
             * @param [in] length 报文体长度
             * @return 转换结果
             */
            inline std::string lenToString(int length);
        public:

            /// 套接字文件描述符与Mutex指针哈希表
            static std::unordered_map<int, std::shared_ptr<Mutex>> m_socket_mutex_map;

            /*!
             * @brief 构造函数
             * @param [in] sock_fd 初始化响应的套接字文件描述符
             */
            explicit Response(int sock_fd);

            /*!
             * @brief 发送响应报文
             * @param [in] body 响应报文体
             */
            void sendResponse(const std::string& body);
        };

        /*!
         * @brief 服务器内部报文处理类 \class
         */
        class PacketProcessor {
        private:

            /// 包处理缓冲区
            char m_buffer[BUFFER_SIZE]{};

            /// 当前处理数据包
            std::string m_packet;

            /// 当前处理包长
            int32_t m_packet_len;

            /// 包长数据缓冲区
            std::string m_packet_len_buf;

            /// 需要执行的用户业务逻辑
            std::function<void(const Request&, Response&)>& m_business_logic;

            /*!
             * @brief 生成单个请求报文
             * @param [in] data_len 缓冲区中数据长度
             * @param [in] sock_fd 对应socket文件描述符
             */
            void generatePacket(int data_len, int sock_fd);

            /*!
             * @brief 获取单个报文的报文长度（包头）
             * @param [in] data_len 缓冲区中数据长度
             */
            void getPacketLen(int data_len);

            /*!
             * @brief 切分报文边界，获取后续报文的包头
             */
            void cutPacketStream();

        public:

            /*!
             * @brief 构造函数
             * @param [in] business_logic 用户业务逻辑函数对象
             */
            explicit PacketProcessor(std::function<void(const Request&, Response&)>& business_logic);

            /*!
             * @brief 读取并处理缓冲区数据
             * @param [in] sock_fd 欲读取的socket文件描述符
             * @return 操作完成状态码，包括 ResetOneShotStatusCode 和 CloseSockFdStatusCode
             */
            int readBuffer(int sock_fd);
        };

        /*!
         * @brief 构造函数
         * @param [in] business_logic 业务逻辑函数对象
         */
        explicit Server(std::function<void(const Request&, Response&)> business_logic);

        /*!
         * @brief 析构函数
         */
        ~Server();

        /*!
         * @brief 终止服务器
         */
        void terminate();

        /*!
         * @brief 运行服务器
         */
        void run();

    private:

        /*!
         * @brief 将文件描述符设置为非阻塞读写模式
         * @param [in] fd 目标文件描述符
         * @return 文件描述符上原来的模式掩码
         */
        int setNonBlocking(int fd);

        /*!
         * @brief 讲文件描述符fd添加到epoll监听列表中
         * @param [in] fd 目标文件描述符
         * @param [in] enable_one_shot 使用EPOLLONESHOT模式（避免线程竞争读fd）
         */
        void addFd(int fd, bool enable_one_shot);

        /*!
         * @brief 对socket文件描述符的EPOLLONESHOT选项进行重置，使后续读事件到来时其他线程可以进行读操作
         * @param [in] fd 目标socket文件描述符
         */
        void resetOneShot(int fd);

        /*!
         * @brief 关闭socket连接
         * @param [in] sock_fd 目标socket文件描述符
         */
        void closeSocketConnection(int sock_fd);

        /*!
         * @brief epoll事件触发函数
         * @param [in] number 就绪文件描述符数目
         */
        void edgeTriggerEventFunc(int number);

        /*!
         * @brief 初始化服务器：包括配置文件加载、服务端监听套接字准备、启动线程池
         */
        void initServer();

        /*!
         * @brief 获取配置文件内容
         */
        void getConfiguration();

        /*!
         * @brief 任务Id生成，用于标识即将加入线程池任务队列的任务
         * @return 任务Id
         */
        inline int32_t generateTaskId();

        /*!
         * @brief 打印调试信息（使用宏 _XJJ_DEBUG 开启调试模式），采用 printf 实现
         * @param [in] format 格式字符指针常量
         * @param [in] ... 指定格式的可变长参数列表
         */
        static void printDebugInfo(const char* format, ...);

        /*!
         * @brief 打印插桩信息，采用 printDebugInfo 实现
         * @param [in] pt_id 插桩编号
         */
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

        /// 用户业务逻辑函数对象
        std::function<void(const Request&, Response&)> m_business_logic;

        /// 线程池对象指针
        std::unique_ptr<ThreadPool> m_thread_pool;

        /// 服务器IP
        std::string m_ip;

        /// 服务器端口
        uint16_t m_port;

        /// 线程池大小
        ThreadPool::thread_num_type m_thread_pool_size;

        /// 线程池是否允许过载
        bool m_thread_pool_overload;

        /// 服务器监听套接字文件描述符
        int m_listen_fd;

        /// 服务器运行状态标识
        bool m_is_running;
    };
} // namespace xjj

#endif