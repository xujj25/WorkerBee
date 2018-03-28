#include <string>
#include <iostream>
#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
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

    static const int
            InsertCmd = 0,
            SelectCmd = 1,
            UpdateCmd = 2,
            DeleteCmd = 3;

    static const int
            ParamErr = 0,
            SQLErr = 1,
            Success = 2,
            Fail = 3;

    sql::ResultSet* executeSQL(const std::string& sql) {
        sql::ResultSet* res = nullptr;
        shared_ptr<sql::Connection> con = nullptr;
        try {
            con = m_conn_pool -> getConnection();
            shared_ptr<sql::Statement> stmt(con -> createStatement());

            cout << sql << endl;

            res = stmt -> executeQuery(sql);

        } catch (sql::SQLException &e) {
            cout << e.what() << endl;
            cout << "(SQLException error code: " << e.getErrorCode() << ")" << endl;
            res = nullptr;
        }
        if (con)
            m_conn_pool -> returnConnection(con);  // 注意返回连接！
        return res;
    }

    int insert(const rapidjson::Document& doc) {
        if (!doc.HasMember("Id") || !doc["Id"].IsInt() ||
                !doc.HasMember("Name") || !doc["Name"].IsString())
            return ParamErr;
        std::string sql = "INSERT INTO Writers(Id, Name) VALUES (";
        sql.append(std::to_string(doc["Id"].GetInt()));
        sql.append(", \'");
        sql.append(doc["Name"].GetString());
        sql.append("\')");

        unique_ptr<sql::ResultSet> res(executeSQL(sql));
        if (res == nullptr)
            return SQLErr;

        if (res -> getAffectedRow() > 0)
            return Success;
        else
            return Fail;
    }

    int select(const rapidjson::Document& doc, rapidjson::Document& res_doc) {
        if (!doc.HasMember("Id") || !doc["Id"].IsInt())
            return ParamErr;
        std::string sql = "SELECT Name FROM Writers WHERE Id = ";
        sql.append(std::to_string(doc["Id"].GetInt()));

        unique_ptr<sql::ResultSet> res(executeSQL(sql));
        if (res == nullptr)
            return SQLErr;
        auto &alloc = res_doc.GetAllocator();
        rapidjson::Value arr(rapidjson::kArrayType);
        while (res -> next()) {
            rapidjson::Value str_obj(rapidjson::kStringType);
            std::string name(res -> getString("Name"));
            str_obj.SetString(name.c_str(), name.length(), alloc);
            arr.PushBack(str_obj, alloc);
        }
        res_doc.AddMember("names", arr, alloc);

        return Success;
    }

    int update(const rapidjson::Document& doc) {
        if (!doc.HasMember("Id") || !doc["Id"].IsInt() ||
            !doc.HasMember("Name") || !doc["Name"].IsString())
            return ParamErr;

        std::string sql = "UPDATE Writers SET Name = \'";
        sql.append(doc["Name"].GetString());
        sql.append("\' WHERE Id = ");
        sql.append(std::to_string(doc["Id"].GetInt()));

        unique_ptr<sql::ResultSet> res(executeSQL(sql));
        if (res == nullptr)
            return SQLErr;

        if (res -> getAffectedRow() > 0)
            return Success;
        else
            return Fail;
    }

    int remove(const rapidjson::Document& doc) {
        if (!doc.HasMember("Id") || !doc["Id"].IsInt())
            return ParamErr;

        std::string sql = "DELETE FROM Writers WHERE Id = ";
        sql.append(std::to_string(doc["Id"].GetInt()));

        unique_ptr<sql::ResultSet> res(executeSQL(sql));
        if (res == nullptr)
            return SQLErr;

        if (res -> getAffectedRow() > 0)
            return Success;
        else
            return Fail;
    }

public:

    BusinessLogic() {
        m_conn_pool = MySQLConnectionPool::getInstance();
    }

    void operator() (Server::Request request, Server::Response response) {
        rapidjson::Document doc;
        doc.Parse(request.getBody().c_str());

        if (!doc.IsObject() || !doc.HasMember("timestamp") || !doc["timestamp"].IsInt64()) {
            response.sendResponse("req_err");
            return;
        }

        rapidjson::Document res_doc;
        res_doc.SetObject();
        auto& alloc = res_doc.GetAllocator();
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        res_doc.AddMember("cli_timestamp", doc["timestamp"].GetInt64(), alloc);

        if (!doc.HasMember("cmd") || !doc["cmd"].IsInt()) {
            res_doc.AddMember("status", "cmd_err", alloc);
            res_doc.Accept(writer);
            response.sendResponse(buffer.GetString());
            return;
        }

        int res_status;

        switch (doc["cmd"].GetInt()) {
            case InsertCmd:
                res_status = insert(doc);
                break;
            case SelectCmd:
                res_status = select(doc, res_doc);
                break;
            case UpdateCmd:
                res_status = update(doc);
                break;
            case DeleteCmd:
                res_status = remove(doc);
                break;
            default:
                res_doc.AddMember("status", "cmd_err", alloc);
                res_doc.Accept(writer);
                response.sendResponse(buffer.GetString());
                return;
        }

        switch (res_status) {
            case ParamErr:
                res_doc.AddMember("status", "param_err", alloc);
                break;
            case SQLErr:
                res_doc.AddMember("status", "sql_err", alloc);
                break;
            case Fail:
                res_doc.AddMember("status", "fail", alloc);
                break;
            case Success:
                res_doc.AddMember("status", "ok", alloc);
                break;
            default:
                res_doc.AddMember("status", "other_err", alloc);
                break;
        }

        res_doc.Accept(writer);
        response.sendResponse(buffer.GetString());
    }
};

int main() {
    BusinessLogic businessLogic;
    unique_ptr<Server> server(new Server(businessLogic));
    server -> run();

    return 0;
}