CC:= g++ -std=c++11 -g -Wall

all: bin/client_test bin/server_test

# compile client side example program
bin/client_test: example/client_test.cpp
	$(CC) -I ./include $^ -o $@

# compile server side example program
bin/server_test: build/condition_variable.o build/mutex.o build/mysql_connection.o \
	build/thread_pool.o build/server.o build/mysql_connection_pool.o build/server_test.o
	$(CC) -I ./include $^ -o $@ -lpthread -lmysqlclient
build/condition_variable.o: include/condition_variable.hpp src/condition_variable.cpp
	$(CC) -I ./include -c src/condition_variable.cpp -o $@
build/mutex.o: include/mutex.hpp src/mutex.cpp
	$(CC) -I ./include -c src/mutex.cpp -o $@
build/mysql_connection.o: include/mysql_connection.hpp src/mysql_connection.cpp
	$(CC) -I ./include -c src/mysql_connection.cpp -o $@
build/thread_pool.o: include/thread_pool.hpp include/blocking_queue.hpp src/thread_pool.cpp
	$(CC) -I ./include -c src/thread_pool.cpp -o $@
build/server.o: include/server.hpp src/server.cpp
	$(CC) -I ./include -c src/server.cpp -o $@
build/mysql_connection_pool.o: include/mysql_connection_pool.hpp src/mysql_connection_pool.cpp
	$(CC) -I ./include -c src/mysql_connection_pool.cpp -o $@
build/server_test.o: example/server_test.cpp
	$(CC) -I ./include -c $^ -o $@

clean:
	@rm -rf build/*.o bin/*
