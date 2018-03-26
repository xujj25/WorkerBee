//
// created by xujijun on 2018-03-20
//

#include <fstream>
#include <mysql_connection_pool.hpp>
#include <rapidjson/document.h>

namespace xjj {

    const std::string MySQLConnectionPool::ConfigFileName = "./config.json";

    MySQLConnectionPool* MySQLConnectionPool::m_instance = nullptr;

    Mutex MySQLConnectionPool::m_instance_mutex;

    MySQLConnectionPool::MySQLConnectionPool() {
        m_driver.reset(sql::Driver::getDriverInstance());

        getConfiguration();

        AutoLockMutex autoLockMutex(&m_list_mutex);
        for (auto count = m_pool_size; count > 0; count--) {
            m_conn_list.push_back(
                    std::shared_ptr<sql::Connection>(m_driver -> connect(m_host, m_user, m_passwd, m_port))
            );
        }
    }

    MySQLConnectionPool* MySQLConnectionPool::getInstance() {
        if (!m_instance) {
            AutoLockMutex autoLockMutex(&m_instance_mutex);
            if (!m_instance)
                m_instance = new MySQLConnectionPool();
        }
        return m_instance;
    }

    void MySQLConnectionPool::getConfiguration() {
        std::ifstream config_fs;
        config_fs.open(ConfigFileName.c_str());


        if (config_fs) {
            std::string exception_msg =
                    "Exception: in function xjj::MySQLConnectionPool::getConfiguration when getting ";
            std::string config_json, pcs;

            while (config_fs >> pcs) {
                config_json.append(pcs);
            }

            rapidjson::Document document;
            document.Parse(config_json.c_str());

            if (!document.IsObject())
                throw std::runtime_error(exception_msg + "JSON object from \"./config.json\"");

            if (!document.HasMember("db_host") || !document["db_host"].IsString())
                throw std::runtime_error(exception_msg + "\"db_host\"");

            if (!document.HasMember("db_user") || !document["db_user"].IsString())
                throw std::runtime_error(exception_msg + "\"db_user\"");

            if (!document.HasMember("db_passwd") || !document["db_passwd"].IsString())
                throw std::runtime_error(exception_msg + "\"db_passwd\"");

            if (!document.HasMember("db_port") || !document["db_port"].IsUint())
                throw std::runtime_error(exception_msg + "\"db_port\"");

            if (!document.HasMember("db_pool_size") || !document["db_pool_size"].IsUint64())
                throw std::runtime_error(exception_msg + "\"db_pool_size\"");

            m_host = document["db_host"].GetString();
            m_user = document["db_user"].GetString();
            m_passwd = document["db_passwd"].GetString();
            m_port = document["db_port"].GetUint();
            m_pool_size = document["db_pool_size"].GetUint64();

        } else {
            throw std::runtime_error("Fail to open \"./config.json\"!");
        }
    }

    MySQLConnectionPool::~MySQLConnectionPool() {
        m_conn_list.clear();
    }

    std::shared_ptr<sql::Connection> MySQLConnectionPool::getConnection() {
        AutoLockMutex autoLockMutex(&m_list_mutex);
        auto conn(std::move(m_conn_list.front()));
        m_conn_list.pop_front();
        return conn;
    }

    void MySQLConnectionPool::returnConnection(
            std::shared_ptr<sql::Connection> conn) {
        AutoLockMutex autoLockMutex(&m_list_mutex);
        m_conn_list.push_back(conn);
    }

} // namespace xjj
