//
// created by xujijun on 2018-03-25
//

#ifndef _XJJ_MYSQL_CONNECTION_HPP
#define _XJJ_MYSQL_CONNECTION_HPP

#include <stdexcept>
#include <string>
#include <mysql/mysql.h>
#include "mutex.hpp"

namespace xjj {
namespace sql {

    class SQLException : public std::runtime_error {
    private:
        unsigned int m_error_code;
    public:
        SQLException(const std::string& msg, unsigned int error_code);

        unsigned int getErrorCode();

        static SQLException generateException(
                MYSQL *mysql,
                const std::string& caller_func,
                const std::string& error_operation
        );
    };

    class ResultSet {
    private:
        int m_row_num;

        uint64_t  m_affect_row_num;

        MYSQL *m_mysql;

        MYSQL_RES *m_result;

        MYSQL_ROW m_row;
    public:

        explicit ResultSet(MYSQL *mysql);

        ~ResultSet();

        bool next();

        uint64_t getAffectedRow() const;

        int getRow() const;

        int getInt(uint32_t column_index) const;

        int getInt(const std::string& column_label) const;

        std::string getString(uint32_t column_index) const;

        std::string getString(const std::string& column_label) const;
    };

    class Statement {
    private:
        MYSQL *m_mysql;
    public:

        explicit Statement(MYSQL *mysql);

        ResultSet* executeQuery(const std::string& sql);
    };

    class Connection {
    private:
        MYSQL *m_mysql;
    public:

        explicit Connection(MYSQL *mysql);

        ~Connection();

        void setSchema(const std::string& catalog);

        Statement* createStatement();
    };

    class Driver {
    private:

        static Mutex m_mutex;

        static Driver* m_instance;

        Driver();

    public:

        ~Driver();

        static Driver* getDriverInstance();

        Connection* connect(
                const std::string& host = "",
                const std::string& user = "",
                const std::string& passwd = "",
                unsigned int port = 0
        );
    };
} // namespace sql
} // namespace xjj

#endif