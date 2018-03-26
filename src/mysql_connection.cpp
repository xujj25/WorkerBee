//
// created by xujijun on 2018-03-25
//

#include <cstring>
#include "mysql_connection.hpp"

namespace xjj {
namespace sql {
    SQLException::SQLException(
            const std::string& msg,
            unsigned int error_code)
            : runtime_error("SQLException: " + msg), m_error_code(error_code) {}

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

    unsigned int SQLException::getErrorCode() {
        return m_error_code;
    }

    ResultSet::ResultSet(MYSQL *mysql)
            : m_mysql(mysql), m_row_num(0) {
        m_result = mysql_store_result(m_mysql);
        if (!m_result)
            throw SQLException::generateException(
                    m_mysql, "xjj::ResultSet::ResultSet", "getting result");
    }

    bool ResultSet::next() {
        if ((m_row = mysql_fetch_row(m_result))) {
            ++m_row_num;
            return true;
        }
        if (mysql_errno(m_mysql))
            throw SQLException::generateException(
                    m_mysql, "xjj::ResultSet::next", "fetching next row");
        return false;
    }

    std::string ResultSet::getString(uint32_t column_index) const {
        return std::string(m_row[column_index - 1]);
    }

    std::string ResultSet::getString(const std::string &column_label) const {
        MYSQL_FIELD *field;
        uint32_t col_index = 1;
        while ((field = mysql_fetch_field(m_result))) {
            if (0 == strcmp(column_label.c_str(), field -> name))
                break;
            ++col_index;
        }
        return getString(col_index);
    }

    int ResultSet::getRow() const {
        return m_row_num;
    }

    int ResultSet::getInt(uint32_t column_index) const {
        return std::stoi(getString(column_index), nullptr, 10);
    }

    int ResultSet::getInt(const std::string &column_label) const {
        return std::stoi(getString(column_label), nullptr, 10);
    }

    ResultSet::~ResultSet() {
        mysql_free_result(m_result);
    }


    Statement::Statement(MYSQL *mysql)
            : m_mysql(mysql) {}

    ResultSet *Statement::executeQuery(const std::string &sql) {
        if (mysql_query(m_mysql, sql.c_str()))
            throw SQLException::generateException(
                    m_mysql, "xjj::sql::Statement::executeQuery", "executing SQL");
        return new ResultSet(m_mysql);
    }

    Connection::Connection(MYSQL *mysql)
            : m_mysql(mysql) {}

    void Connection::setSchema(const std::string &catalog) {
        if (mysql_select_db(m_mysql, catalog.c_str()))
            throw SQLException::generateException(
                    m_mysql, "xjj::sql::Connection::setSchema", "selecting db");
    }

    Statement *Connection::createStatement() {
        return new Statement(m_mysql);
    }

    Connection::~Connection() {
        mysql_close(m_mysql);
    }

    Driver::Driver() {
//        m_mysql = mysql_init(nullptr);
    }

    Driver::~Driver() {
//        mysql_close(m_mysql);
    }

    Driver* Driver::m_instance = nullptr;

    Mutex Driver::m_mutex;

    Driver *Driver::getDriverInstance() {
        if (!m_instance) {
            AutoLockMutex autoLockMutex(&m_mutex);
            if (!m_instance)
                m_instance = new Driver();
        }
        return m_instance;
    }

    Connection *Driver::connect(
            const std::string &host,
            const std::string &user,
            const std::string &passwd,
            unsigned int port) {

        MYSQL *mysql = mysql_init(nullptr);

        if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), passwd.c_str(),
                               nullptr, port, nullptr, 0))
            throw SQLException::generateException(
                    mysql, "xjj::sql::Driver::connect", "connecting");
        return new Connection(mysql);
    }
} // namespace sql
} // namespace xjj