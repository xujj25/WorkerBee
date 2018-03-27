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

        static const std::string ConfigFileName;

        static Mutex m_instance_mutex;

        static MySQLConnectionPool* m_instance;

        std::unique_ptr<sql::Driver> m_driver;

        Mutex m_list_mutex;

        std::list<std::shared_ptr<sql::Connection>> m_conn_list;

        std::string m_host;

        std::string m_user;

        std::string m_passwd;

        std::string m_db_name;

        unsigned int m_port;

        size_t m_pool_size;

        void getConfiguration();

        MySQLConnectionPool();

    public:

        ~MySQLConnectionPool();

        static MySQLConnectionPool* getInstance();

        std::shared_ptr<sql::Connection> getConnection();

        void returnConnection(std::shared_ptr<sql::Connection> conn);

    };
} // namespace xjj

#endif