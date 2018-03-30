//
// created by xujijun on 2018-03-20
//

#include <fstream>
#include <mysql_connection_pool.hpp>
#include <rapidjson/document.h>

namespace xjj {

    const std::string MySQLConnectionPool::ConfigFileName = "./config.json";

    std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::m_instance = nullptr;

    Mutex MySQLConnectionPool::m_instance_mutex;

    /*!
     * @brief 构造函数
     */
    MySQLConnectionPool::MySQLConnectionPool()
            : m_pool_size(5), m_port(0) {
        m_driver = sql::Driver::getDriverInstance();

        getConfiguration();  // 获取配置文件配置信息

        // 加锁初始化连接池
        AutoLockMutex autoLockMutex(&m_list_mutex);
        for (auto count = m_pool_size; count > 0; count--) {
            m_conn_list.push_back(
                    m_driver -> connect(m_host, m_user, m_passwd, m_db_name, m_port));
        }
    }

    /*!
     * @brief 获取连接池全局单例
     * @return 连接池对象
     */
    std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::getInstance() {
        if (!m_instance) {
            AutoLockMutex autoLockMutex(&m_instance_mutex);
            if (!m_instance)
                m_instance = std::shared_ptr<MySQLConnectionPool>(new MySQLConnectionPool());
        }
        return m_instance;
    }

    /*!
     * @brief 获取配置文件内容
     */
    void MySQLConnectionPool::getConfiguration() {
        std::ifstream config_fs;
        config_fs.open(ConfigFileName.c_str());


        if (config_fs) {
            std::string exception_msg =
                    "Exception: in function xjj::MySQLConnectionPool::getConfiguration when getting ";
            std::string config_json, pcs;

            while (config_fs >> pcs) {  // 获取文件内容
                config_json.append(pcs);
            }

            // 使用RapidJSON解析配置信息
            rapidjson::Document document;
            document.Parse(config_json.c_str());

            // 解析结果合法性判定
            if (!document.IsObject())
                throw std::runtime_error(exception_msg + "JSON object from \"./config.json\"");

            if (!document.HasMember("db_host") || !document["db_host"].IsString())
                throw std::runtime_error(exception_msg + "\"db_host\"");

            if (!document.HasMember("db_user") || !document["db_user"].IsString())
                throw std::runtime_error(exception_msg + "\"db_user\"");

            if (!document.HasMember("db_passwd") || !document["db_passwd"].IsString())
                throw std::runtime_error(exception_msg + "\"db_passwd\"");

            m_host = document["db_host"].GetString();
            m_user = document["db_user"].GetString();
            m_passwd = document["db_passwd"].GetString();

            // 以下为可选配置项
            if (document.HasMember("db_name")) {
                if (!document["db_name"].IsString())
                    throw std::runtime_error(exception_msg + "\"db_name\"");
                m_db_name = document["db_name"].GetString();
            }

            if (document.HasMember("db_port")) {
                if (!document["db_port"].IsUint())
                    throw std::runtime_error(exception_msg + "\"db_port\"");
                m_port = document["db_port"].GetUint();
            }

            if (document.HasMember("db_pool_size")) {
                if (!document["db_pool_size"].IsUint64())
                    throw std::runtime_error(exception_msg + "\"db_pool_size\"");
                m_pool_size = document["db_pool_size"].GetUint64();
            }

        } else {
            throw std::runtime_error("Fail to open \"./config.json\"!");
        }
    }

    /*!
     * @brief 析构函数
     */
    MySQLConnectionPool::~MySQLConnectionPool() {
        m_conn_list.clear();  // 清空连接池
    }

    /*!
     * @brief 获取连接对象
     * @return 连接对象
     */
    std::shared_ptr<sql::Connection> MySQLConnectionPool::getConnection() {
        AutoLockMutex autoLockMutex(&m_list_mutex);
        std::shared_ptr<sql::Connection> conn(std::move(m_conn_list.front()));
        m_conn_list.pop_front();
        return conn;
    }

    /*!
     * @brief 返还连接
     * @param [in] conn 目标连接对象
     */
    void MySQLConnectionPool::returnConnection(
            std::shared_ptr<sql::Connection> conn) {
        AutoLockMutex autoLockMutex(&m_list_mutex);
        m_conn_list.push_back(conn);
    }

} // namespace xjj
