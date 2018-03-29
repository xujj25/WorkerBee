//
// created by xujijun on 2018-03-20
//

#ifndef _XJJ_MYSQL_CONNECTION_POOL_HPP
#define _XJJ_MYSQL_CONNECTION_POOL_HPP

#include <memory>
#include <list>
#include "mutex.hpp"
#include "mysql_connection.hpp"

namespace xjj {

    /*!
     * @brief MySQL数据库连接池类 \class
     */
    class MySQLConnectionPool {
    private:

        /// 配置文件名
        static const std::string ConfigFileName;

        /// 用于获取连接池单例的互斥量
        static Mutex m_instance_mutex;

        /// 连接池全局单例
        static std::shared_ptr<MySQLConnectionPool> m_instance;

        /// 数据库驱动对象
        std::shared_ptr<sql::Driver> m_driver;

        /// 用于获取连接池中连接的互斥量
        Mutex m_list_mutex;

        /// 存放连接对象指针的双头链表
        std::list<std::shared_ptr<sql::Connection>> m_conn_list;

        /// 数据库地址
        std::string m_host;

        /// 用户名
        std::string m_user;

        /// 密码
        std::string m_passwd;

        /// 数据库名
        std::string m_db_name;

        /// 连接端口
        unsigned int m_port;

        /// 连接池大小
        size_t m_pool_size;

        /*!
         * @brief 获取配置文件内容
         */
        void getConfiguration();

        /*!
         * @brief 构造函数
         */
        MySQLConnectionPool();

    public:

        /*!
         * @brief 析构函数
         */
        ~MySQLConnectionPool();

        /*!
         * @brief 获取连接池全局单例
         * @return 连接池对象
         */
        static std::shared_ptr<MySQLConnectionPool> getInstance();

        /*!
         * @brief 获取连接对象
         * @return 连接对象
         */
        std::shared_ptr<sql::Connection> getConnection();

        /*!
         * @brief 返还连接
         * @param [in] conn 目标连接对象
         */
        void returnConnection(std::shared_ptr<sql::Connection> conn);

    };
} // namespace xjj

#endif