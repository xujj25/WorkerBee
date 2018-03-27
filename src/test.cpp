#include <iostream>
#include <memory>
#include <string>
#include <rapidjson/document.h>
#include "server.hpp"
#include "mysql_connection_pool.hpp"

using std::cout;
using std::endl;
using std::unique_ptr;
using std::shared_ptr;
using namespace xjj;

class BusinessLogic {
private:
    MySQLConnectionPool* m_conn_pool;

public:

    BusinessLogic() {
        m_conn_pool = MySQLConnectionPool::getInstance();
    }

    void operator() (xjj::Server::Request request, xjj::Server::Response response) {

        cout << "Request:" << endl;
        cout << request.getBody() << endl;

        response.sendResponse("ok");

//        rapidjson::Document doc;
//        doc.Parse(request.getBody().c_str());
//
//        // 客户端数据合法性校验
//        if (!doc.IsObject() || !doc.HasMember("Id") || !doc["Id"].IsUint()) {
//            response.sendResponse("Error!");
//            return;
//        }
//
//        std::string sql = "INSERT INTO Writers(Id, Name) VALUES(";
//        sql.append(std::to_string(doc["Id"].GetUint()));
//        sql.append(", \'");
//        sql.append(doc["Name"].GetString());
//        sql.append("\')");
//
//        cout << endl;
//        cout << sql << endl;
//
//        try {
////            unique_ptr<sql::Driver> driver;
//            shared_ptr<sql::Connection> con;
//            unique_ptr<sql::Statement> stmt;
//            unique_ptr<sql::ResultSet> res;
//
//            con = m_conn_pool -> getConnection();
//
//            /* Connect to the MySQL testdb database */
//            con -> setSchema("testdb");
//
//            stmt.reset(con->createStatement());
//
//            res.reset(stmt->executeQuery(sql));
//
//            std::string res_msg = "Affected rows: ";
//
//            res_msg.append(std::to_string(res -> getAffectedRow()));
//
//            response.sendResponse(res_msg);
//
//            m_conn_pool -> returnConnection(con);
//
//        } catch (sql::SQLException &e) {
//
//            cout << "# ERR: " << e.what() << endl;
//            cout << " (MySQL error code: " << e.getErrorCode() << ")";
//
//            response.sendResponse("Error!");
//        }
//
//        cout << endl;
    }
};

int main() {
    BusinessLogic businessLogic;
    std::unique_ptr<xjj::Server> server(new xjj::Server(businessLogic));
    server -> run();

    return 0;
}