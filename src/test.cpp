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

class Test {
private:
    MySQLConnectionPool* m_conn_pool;
public:

    Test() {
        m_conn_pool = MySQLConnectionPool::getInstance();
    }

    ~Test() {
        delete m_conn_pool;
    }

    void businessLogic(xjj::Server::Request request, xjj::Server::Response response) {

        rapidjson::Document doc;
        doc.Parse(request.getBody().c_str());

        if (!doc.IsObject() || !doc.HasMember("country_id") || !doc["country_id"].IsUint()) {
            response.sendResponse("Error!");
            return;
        }

        std::string sql = "SELECT * FROM country WHERE country_id = ";
        sql.append(std::to_string(doc["country_id"].GetUint()));

        cout << endl;
        cout << sql << endl;

        try {
//            unique_ptr<sql::Driver> driver;
            shared_ptr<sql::Connection> con;
            unique_ptr<sql::Statement> stmt;
            unique_ptr<sql::ResultSet> res;

            con = m_conn_pool -> getConnection();

            /* Connect to the MySQL testdb database */
            con -> setSchema("testdb");

            stmt.reset(con->createStatement());

            res.reset(stmt->executeQuery(sql));
            while (res->next()) {
//                cout << "\t... MySQL replies: ";
//                /* Access column data by alias or column name */
//                cout << res->getString("country") << endl;
//                cout << "\t... MySQL says it again: ";
//                /* Access column data by numeric offset, 1 is the first column */
//                cout << res->getInt(1) << endl;
                response.sendResponse(res -> getString("country"));
            }

            m_conn_pool -> returnConnection(con);

        } catch (sql::SQLException &e) {

            cout << "# ERR: " << e.what() << endl;
            cout << " (MySQL error code: " << e.getErrorCode() << ")";
        }

        cout << endl;
    }
};

int main() {
    Test test;
    std::unique_ptr<xjj::Server> server(new xjj::Server(
            [&] (Server::Request request, Server::Response response) {
                test.businessLogic(request, response);
            }
    ));
    server -> run();

    return 0;
}