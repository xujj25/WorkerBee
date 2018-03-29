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

/*!
 * @brief MySQL操作相关类命名空间 \namespace
 */
namespace sql {

    /*!
     * @brief SQL异常类 \class
     */
    class SQLException : public std::runtime_error {
    private:

        /// 错误编号
        unsigned int m_error_code;

    public:

        /*!
         * @brief 构造函数
         * @param [in] msg 错误信息
         * @param [in] error_code 错误编号
         */
        SQLException(const std::string& msg, unsigned int error_code);

        /*!
         * @brief 获取错误编号
         * @return 错误编号
         */
        unsigned int getErrorCode();

        /*!
         * @brief 生成异常
         * @param [in] mysql 数据库连接句柄
         * @param [in] caller_func 调用函数名
         * @param [in] error_operation 出现错误的操作
         * @return 所生成的异常
         */
        static SQLException generateException(
                MYSQL *mysql,
                const std::string& caller_func,
                const std::string& error_operation
        );
    };

    class Connection;
    class Statement;
    class ResultSet;

    /*!
     * @brief 数据库连接驱动
     */
    class Driver {
    private:

        /// 用于获取驱动单例的互斥量
        static Mutex m_mutex;

        /// 驱动全局单例
        static std::shared_ptr<Driver> m_instance;

        /*!
         * @brief 构造函数，设置为私有，用户只能通过静态函数getDriverInstance 获取全局单例
         */
        Driver() = default;

    public:

        /*!
         * @brief 驱动全局单例获取函数
         * @return 驱动全局单例
         */
        static std::shared_ptr<Driver> getDriverInstance();

        /*!
         * @brief 建立数据库连接
         * @param [in] host 数据库地址
         * @param [in] user 用户名
         * @param [in] passwd 密码
         * @param [in] db 所选数据库名
         * @param [in] port 数据库端口
         * @return 连接实例指针
         */
        std::shared_ptr<Connection> connect(
                const std::string& host = "",
                const std::string& user = "",
                const std::string& passwd = "",
                const std::string& db = "",
                unsigned int port = 0
        );
    };

    /*!
     * @brief 数据库连接类 \class
     */
    class Connection {
        // 将驱动类中的连接获取函数声明为连接类的友元，可以访问连接类的构造函数
        friend std::shared_ptr<Connection> Driver::connect(
                const std::string& host,
                const std::string& user,
                const std::string& passwd,
                const std::string& db,
                unsigned int port
        );

    private:

        /// 数据库连接句柄
        MYSQL *m_mysql;

        /*!
         * @brief 构造函数
         * @param [in] mysql 连接句柄
         */
        explicit Connection(MYSQL *mysql);

    public:

        /*!
         * @brief 析构函数
         */
        ~Connection();

        /*!
         * @brief 设置Schema（用于选择数据库）
         * @param [in] catalog Schema内容
         */
        void setSchema(const std::string& catalog);

        /*!
         * @brief 创建sql表达式
         * @return sql表达式指针
         */
        std::shared_ptr<Statement> createStatement();
    };

    /*!
     * @brief sql表达式类 \class
     */
    class Statement {

        // 将连接类的sql表达式类创建函数声明为友元，可以访问sql表达式类的构造函数
        friend std::shared_ptr<Statement> Connection::createStatement();

    private:

        /// 数据库连接句柄
        MYSQL *m_mysql;

        /*!
         * @brief 构造函数
         * @param [in] mysql 连接句柄
         */
        explicit Statement(MYSQL *mysql);

    public:

        /*!
         * @brief 执行sql语句
         * @param [in] sql sql语句
         * @return 结果集对象指针
         */
        std::shared_ptr<ResultSet> executeQuery(const std::string& sql);
    };

    /*!
     * @brief sql执行结果集类 \class
     */
    class ResultSet {

    // 将sql表达式类的sql执行函数声明为友元，可以访问结果集类的构造函数
    friend std::shared_ptr<ResultSet> Statement::executeQuery(const std::string &sql);

    private:

        /// 执行结果行号（SELECT操作）
        int m_row_num;

        /// 语句影响行数（非SELECT操作）
        uint64_t  m_affect_row_num;

        /// 数据库连接句柄
        MYSQL *m_mysql;

        /// 底层结果集对象指针
        MYSQL_RES *m_result;

        /// 当前指向结果行
        MYSQL_ROW m_row;

        /*!
         * @brief 构造函数
         * @param [in] mysql 连接句柄
         */
        explicit ResultSet(MYSQL *mysql);

    public:

        /*!
         * @brief 析构函数
         */
        ~ResultSet();

        /*!
         * @brief 将结果光标移动到下一行
         * @return
         */
        bool next();

        /*!
         * @brief 获取sql操作影响行数
         * @return 影响行数
         */
        uint64_t getAffectedRow() const;

        /*!
         * @brief 获取当前光标指向行行号
         * @return
         */
        int getRow() const;

        /*!
         * @brief 根据列号获取列中int数据
         * @param [in] column_index 列号
         * @return 获取到的数据
         */
        int getInt(uint32_t column_index) const;

        /*!
         * @brief 根据列名获取列中int数据
         * @param [in] column_label 列名
         * @return 获取到的数据
         */
        int getInt(const std::string& column_label) const;

        /*!
         * @brief 根据列号获取列中的string数据
         * @param [in] column_index 列号
         * @return 获取到的数据
         */
        std::string getString(uint32_t column_index) const;

        /*!
         * @brief 根据列名获取列中的string数据
         * @param [in] column_label 列名
         * @return 获取到的数据
         */
        std::string getString(const std::string& column_label) const;
    };
} // namespace sql
} // namespace xjj

#endif