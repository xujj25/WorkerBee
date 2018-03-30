# Epoll Multithread Server

本项目是一个采用C++编写的TCP socket多线程服务器。

## 项目结构

- 服务器 `Server`
    - 服务器内部报文包处理类 `PacketProcessor`
    - 服务器内部请求类 `Request`
    - 服务器内部响应类 `Response`
- 线程池 `ThreadPool`
    - 线程池内部线程类 `Thread`
    - 阻塞队列模板类 `BlockingQueue`
    - 互斥量类 `Mutex`（以及基于对 `Mutex` 的RAII封装类 `AutoLockMutex`）
    - 条件变量类 `ConditionVariable`
- MySQL数据库连接池 `MySQLConnectionPool`
    - 数据库驱动类 `Driver`
    - 数据库连接类 `Connection`
    - 数据库表达式类 `Statement`
    - 数据库结果集类 `ResultSet`
    - 数据库异常类 `SQLException`

## 运行逻辑

1. 客户端与服务器建立TCP连接。
2. 服务器采用`epoll`监听连接事件和文件描述符可读事件。
3. 当`epoll`返回就绪可读文件描述符时，封装成任务交给线程池处理，任务内容为：
    - 一次性读取出所有可读数据（`epoll`采用`ET`模式）。
    - 对读取到的数据流进行包拆分（后续讲解封包细节），每个包代表一个业务请求。
    - 将业务请求交给用户的业务逻辑处理（具体的业务逻辑采用函数对象传给服务器，仅通过请求和响应对象参数与服务器底层进行交互）。
    - 将业务处理结果发送给客户端。
    - 若客户端已经断开连接，则服务器也断开连接。
    - 结束线程任务。

## 封包操作详解

### 简述

本项目实现了一个简单的应用层协议，客户端和服务器之间进行数据传输时，为每个请求加上“包头”，“包头”的内容即“包体”的长度，以小端序表示，占用4个字节。示意图如下：

```plain
+--------------------+
|       Header       |
|     (4-byte,       |
|  little-endian)    |
+--------------------+
|                    |
|                    |
|                    |
|        Body        |
|                    |
|                    |
|                    |
+--------------------+
```

### 目的

解决客户端在进行连接重用的时候潜在的TCP粘包问题。

## 关于线程竞争连接读写的问题

### 读操作的处理

针对线程竞争的问题，读操作部分采用了`epoll`的`EPOLLONESHOT`选项，具体操作为：

1. 当`epoll`返回新建立连接的套接字文件描述符时，对文件描述符进行`EPOLLONESHOT`设置。
2. 当`epoll`返回可读文件描述符时，线程池中工作线程对描述符进行数据读取。此时在`EPOLLONESHOT`的作用下，当文件描述符上新的读事件到来时，`epoll`不会进行提醒。
3. 当读操作完成时，对文件描述符进行`EPOLLONESHOT`重置，保证后续读事件会被返回。这样就保证了同一时间内对同一文件描述符只可能有一个线程在进行读操作。

### 写操作的处理

写操作部分使用了加锁的方式，具体的实现为：

1. 服务器使用一个`unordered_map`（底层实现为哈希表，具有较快的查找速度），为每一个连接的文件描述符匹配一个互斥量。
2. 利用同一时间在一个连接上只有一个工作线程进行读操作的特性，在读操作之前查看互斥量哈希表中是否存在对应连接的互斥量，若无则插入一个新的互斥量。
3. 当对连接进行写操作的时候，需要先在哈希表中获取对应连接的互斥量（即加锁操作）。
4. 当服务端关闭连接时，删除对应连接的互斥量。

## 线程池部分说明

参见本人项目[ThreadPool](https://github.com/xujj25/ThreadPool)。

## 数据库连接池部分说明

1. 本项目中的数据库组件类是对MySQL的C语言API的封装。
2. 加入了针对非法操作的异常抛出。
3. 接口模仿JDBC。

## 项目环境及依赖

- 操作系统：Ubuntu 16.04
- 语言：C++（采用C++11标准）
- 编译器：g++ 5.4.0
- 自动化构建工具：GNU Make 4.1
- 线程库：POSIX线程库
- MySQL API：MySQL官方C语言API
- 其他依赖：[RapidJSON](http://rapidjson.org/zh-cn/)（用于解析配置文件）

## 使用指南

1. 按照项目环境及依赖配置相关内容。

2. 获取本项目：

    ```bash
    git clone https://github.com/xujj25/epoll-multithread-server.git
    ```

3. 进入项目主目录，添加配置文件`config.json`，字段包括：
    - `ip`：服务器IP地址
    - `port`：服务器开启端口
    - `thread_pool_size`：线程池大小（可选项，默认为5）
    - `thread_pool_overload`：是否允许线程池过载，即是否允许池中总任务量大于工作线程数目（可选项，默认为 `true`）
    - `db_host`：MySQL数据库地址
    - `db_user`：数据库用户名
    - `db_passwd`：数据库密码
    - `db_name`：数据库名（可选项，可在数据库建立连接后调用 `setSchema` 进行设置）
    - `db_port`：数据库端口（可选项，缺省情况下会自动选择MySQL服务器的监听端口）
    - `db_pool_size`：数据库连接池大小，建议与线程池大小相同（可选项，默认为5）
   
   完整示例如下：
    ```JSON
    {
        "ip": "127.0.0.1",
        "port": 1234,
        "thread_pool_size": 5,
        "thread_pool_overload": true,
        "db_host": "127.0.0.1",
        "db_user": "username",
        "db_passwd": "password",
        "db_name": "testdb",
        "db_port": 3306,
        "db_pool_size": 5
    }
    ```
4. 编译运行[示例程序](https://github.com/xujj25/epoll-multithread-server/tree/master/example)：
    - 自动构建客户端和服务端示例程序：
        ```bash
        make
        ```
    - 服务端：
        - 在MySQL中建立数据库表`Writers`：
        ```SQL
        CREATE TABLE `Writers` (
            `Id` int(11) NOT NULL AUTO_INCREMENT,
            `Name` text,
            PRIMARY KEY (`Id`)
        ) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8；
        ```
        - 运行：
        ```bash
        ./bin/server_test
        ```
    - 客户端：
        - 运行：
        ```bash
        ./bin/client_test
        ```

5. 用户代码使用指南：
    - 服务端：
        ```cpp
        #include <memory>
        #include "server.hpp"

        int main() {
            std::unique_ptr<xjj::Server> server(new xjj::Server(
                    [] (const xjj::Server::Request& req,
                        xjj::Server::Response& res) {

                        std::string req_body = req.getBody();
                        std::string res_body;

                        // 此处编写用户业务逻辑代码

                        res.sendResponse(res_body);
                    }
            ));
            server -> run();
        }
        ```
        之后模仿示例代码修改Makefile之后即可编译运行。
    - 客户端：遵循上述封包操作即可。