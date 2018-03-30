#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>
#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

using namespace rapidjson;

#define BUF_SIZE 1000

struct Param
{
    unsigned int Id;
    std::string Name;
};

std::string lenToString(int length) {
    std::string res(4, '\0');
    for (int i = 0; i < 4; i++) {
        res[i] = static_cast<char>(length >> (i * 8));
    }
    return res;
}

void insert(Param& param) {
    printf("Input Id: ");
    scanf("%d", &param.Id);
    printf("Input Name: ");
    std::cin >> param.Name;
}

void update(Param& param) {
    printf("Input Id: ");
    scanf("%d", &param.Id);
    printf("Input Name: ");
    std::cin >> param.Name;
}

void select(Param& param) {
    printf("Input Id: ");
    scanf("%d", &param.Id);
}

void remove(Param& param) {
    printf("Input Id: ");
    scanf("%d", &param.Id);
}

int main() {
	// send request to specified server
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; // use IPv4
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP addr
	serv_addr.sin_port = htons(1234); // port

	char buf_send[BUF_SIZE] = {0};
	char buf_recv[BUF_SIZE] = {0};
	
    // creat socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    
    while (1) {
        std::string cmd;

        std::cout << std::endl;
        std::cout << "-----------" << std::endl;
        std::cout << "0 - insert" << std::endl;
        std::cout << "1 - select" << std::endl;
        std::cout << "2 - update" << std::endl;
        std::cout << "3 - delete" << std::endl;        
        std::cout << "-----------" << std::endl;
        std::cout << "Input choice: " << std::endl;

        std::cin >> cmd;

        Param param;
        bool flag = true;

        switch (cmd[0] - '0') {
            case 0:
                insert(param);
                break;
            case 1:
                select(param);
                break;
            case 2:
                update(param);
                break;
            case 3:
                remove(param);
                break;
            default:
                std::cout << "Wrong command!" << std::endl;
                flag = false;
                break;
        }

        if (!flag)
            continue;

        Document doc;
        doc.SetObject();

        auto& allocator = doc.GetAllocator();

        doc.AddMember("timestamp", 
            static_cast<int64_t>(std::time(nullptr)), allocator);

        doc.AddMember("cmd", static_cast<int>(cmd[0] - '0'), allocator);

        doc.AddMember("Id", param.Id, allocator);

        if (!param.Name.empty()) {
            doc.AddMember("Name", "", allocator);
            doc["Name"].SetString(param.Name.c_str(), param.Name.length(), allocator);
        }

        StringBuffer doc_buf;

        PrettyWriter<StringBuffer> writer(doc_buf);

        doc.Accept(writer);

        std::cout << "Going to send:" << std::endl;

        std::string body = doc_buf.GetString();

        std::cout << body << std::endl;

        std::string request(std::move(lenToString(body.length())));
        request.append(body);
        send(sock, request.c_str(), request.length(), MSG_NOSIGNAL);

		// read data from server
		recv(sock, buf_recv, BUF_SIZE, 0);
		

        // parsing data from server into JSON obj
        doc.Parse(buf_recv + 4);
        if (doc.IsObject() && 
            doc.HasMember("cli_timestamp") && 
            doc["cli_timestamp"].IsInt64() &&
            doc.HasMember("status") &&
            doc["status"].IsString()) {

            printf("Message from server:\n");
            printf("cli_timestamp: %ld\n", doc["cli_timestamp"].GetInt64());
            printf("status: %s\n", doc["status"].GetString());

            if (cmd[0] - '0' == 1) {
                if (doc.HasMember("names") && doc["names"].IsArray()) {
                    printf("Names:\n");
                    for (const auto& name : doc["names"].GetArray()) {
                        printf("\t%s\n", name.GetString());
                    }
                } else {
                    printf("Select Error!\n");
                }
            }

        } else {
            printf("Response Error!\n");
        }
		
		memset(buf_send, 0, BUF_SIZE); // reset send buffer
		memset(buf_recv, 0, BUF_SIZE); // reset recv buffer
    }

	// close socket
	close(sock);
    
	return 0;
}