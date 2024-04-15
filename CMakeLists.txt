cmake_minimum_required(VERSION 3.23)
project(GuoWebServer)

set(CMAKE_CXX_STANDARD 14)

add_executable(GuoWebServer log/log.cpp log/log.h log/block_queue.h http/http_conn.cpp http/http_conn.h threadpool/threadpool.h CGImysql/sql_connection_pool.cpp CGImysql/sql_connection_pool.h)
