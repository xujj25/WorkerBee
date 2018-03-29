//
// created by xujijun on 2018-03-25
//

#include <cstring>
#include "mysql_connection.hpp"

namespace xjj {
namespace sql {

    /*!
     * @brief 构造函数
     * @param [in] msg 错误信息
     * @param [in] error_code 错误编号
     */
    SQLException::SQLException(
            const std::string& msg,
            unsigned int error_code)
            : runtime_error("SQLException: " + msg), m_error_code(error_code) {}

    /*!
     * @brief 生成异常
     * @param [in] mysql 数据库连接句柄
     * @param [in] caller_func 调用函数名
     * @param [in] error_operation 出现错误的操作
     * @return 所生成的异常
     */
    SQLException SQLException::generateException(
            MYSQL *mysql,
            const std::string& caller_func,
            const std::string& error_operation
    ) {
        std::string msg(mysql_error(mysql));
        msg.append(" (in function ");
        msg.append(caller_func);
        msg.append(" when ");
        msg.append(error_operation);
        msg.append(")");
        auto res = SQLException(msg, mysql_errno(mysql));
        return res;
    }

    /*!
     * @brief 获取错误编号
     * @return 错误编号
     */
    unsigned int SQLException::getErrorCode() {
        return m_error_code;
    }

    /*!
     * @brief 构造函数
     * @param [in] mysql 连接句柄
     */
    ResultSet::ResultSet(MYSQL *mysql)
            : m_mysql(mysql), m_row_num(0) {
        m_result = mysql_store_result(m_mysql);
        if (!m_result) {
            if (mysql_field_count(m_mysql) == 0) {
                // sql执行未返回数据，不是SELECT操作
                m_affect_row_num = mysql_affected_rows(m_mysql);
            } else {
                // 出错，抛出异常
                throw SQLException::generateException(
                        m_mysql, "xjj::ResultSet::ResultSet", "getting result");
            }
        }
    }

    /*!
     * @brief 将结果光标移动到下一行
     * @return
     */
    bool ResultSet::next() {
        if ((m_row = mysql_fetch_row(m_result))) {
            ++m_row_num;
            return true;
        }
        if (mysql_errno(m_mysql))  // 判断是否出现异常
            throw SQLException::generateException(
                    m_mysql, "xjj::ResultSet::next", "fetching next row");
        return false;
    }

    /*!
     * @brief 根据列号获取列中的string数据
     * @param [in] column_index 列号
     * @return 获取到的数据
     */
    std::string ResultSet::getString(uint32_t column_index) const {
        return std::string(m_row[column_index - 1]);
    }

    /*!
    * @brief 根据列名获取列中的string数据
    * @param [in] column_label 列名
    * @return 获取到的数据
    */
    std::string ResultSet::getString(const std::string &column_label) const {
        MYSQL_FIELD *field;
        uint32_t col_index = 1;

        // 根据列名获取列号
        while ((field = mysql_fetch_field(m_result))) {
            if (0 == strcmp(column_label.c_str(), field -> name))
                break;
            ++col_index;
        }
        return getString(col_index);
    }

    /*!
     * @brief 获取当前光标指向行行号
     * @return
     */
    int ResultSet::getRow() const {
        return m_row_num;
    }

    /*!
     * @brief 根据列号获取列中int数据
     * @param [in] column_index 列号
     * @return 获取到的数据
     */
    int ResultSet::getInt(uint32_t column_index) const {
        return std::stoi(getString(column_index), nullptr, 10);
    }

    /*!
     * @brief 根据列名获取列中int数据
     * @param [in] column_label 列名
     * @return 获取到的数据
     */
    int ResultSet::getInt(const std::string &column_label) const {
        return std::stoi(getString(column_label), nullptr, 10);
    }

    /*!
     * @brief 获取sql操作影响行数
     * @return 影响行数
     */
    uint64_t ResultSet::getAffectedRow() const {
        return m_affect_row_num;
    }

    /*!
     * @brief 析构函数
     */
    ResultSet::~ResultSet() {
        mysql_free_result(m_result);
    }

    /*!
     * @brief 构造函数
     * @param [in] mysql 连接句柄
     */
    Statement::Statement(MYSQL *mysql)
            : m_mysql(mysql) {}

    /*!
     * @brief 执行sql语句
     * @param [in] sql sql语句
     * @return 结果集对象指针
     */
    std::shared_ptr<ResultSet> Statement::executeQuery(const std::string &sql) {
        if (mysql_query(m_mysql, sql.c_str()))
            throw SQLException::generateException(
                    m_mysql, "xjj::sql::Statement::executeQuery", "executing SQL");
        return std::shared_ptr<ResultSet>(new ResultSet(m_mysql));
    }

    /*!
     * @brief 构造函数
     * @param [in] mysql 连接句柄
     */
    Connection::Connection(MYSQL *mysql)
            : m_mysql(mysql) {}

    /*!
     * @brief 设置Schema（用于选择数据库）
     * @param [in] catalog Schema内容
     */
    void Connection::setSchema(const std::string &catalog) {
        if (mysql_select_db(m_mysql, catalog.c_str()))
            throw SQLException::generateException(
                    m_mysql, "xjj::sql::Connection::setSchema", "selecting db");
    }

    /*!
     * @brief 创建sql表达式
     * @return sql表达式指针
     */
    std::shared_ptr<Statement> Connection::createStatement() {
        return std::shared_ptr<Statement>(new Statement(m_mysql));
    }

    /*!
     * @brief 析构函数
     */
    Connection::~Connection() {
        mysql_close(m_mysql);
    }

    std::shared_ptr<Driver> Driver::m_instance = nullptr;

    Mutex Driver::m_mutex;

    /*!
     * @brief 驱动全局单例获取函数
     * @return 驱动全局单例
     */
    std::shared_ptr<Driver> Driver::getDriverInstance() {
        if (!m_instance) {
            AutoLockMutex autoLockMutex(&m_mutex);
            if (!m_instance)
                m_instance = std::shared_ptr<Driver>(new Driver());
        }
        return m_instance;
    }

    /*!
     * @brief 建立数据库连接
     * @param [in] host 数据库地址
     * @param [in] user 用户名
     * @param [in] passwd 密码
     * @param [in] db 所选数据库名
     * @param [in] port 数据库端口
     * @return 连接实例指针
     */
    std::shared_ptr<Connection> Driver::connect(
            const std::string &host,
            const std::string &user,
            const std::string &passwd,
            const std::string &db,
            unsigned int port) {

        MYSQL *mysql = mysql_init(nullptr);

        if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str(),
                               db.c_str(), port, nullptr, 0))
            throw SQLException::generateException(
                    mysql, "xjj::sql::Driver::connect", "connecting");
        return std::shared_ptr<Connection>(new Connection(mysql));
    }

} // namespace sql
} // namespace xjj