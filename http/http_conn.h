//
// Created by 19851 on 2024/3/22.
//

#ifndef GUOWEBSERVER_HTTP_CONN_H
#define GUOWEBSERVER_HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn {
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    //每个枚举常量都表示了 HTTP 请求的不同状态码，用于表示 HTTP 请求处理的结果或者服务器与客户端之间的通信状态
    enum HTTP_CODE{
        NO_REQUEST, //表示没有接收到完整的 HTTP 请求。
        GET_REQUEST, //表示接收到了完整的 GET 请求。
        BAD_REQUEST, //表示客户端发送的请求有误，无法被服务器理解。
        NO_RESOURCE, //表示请求的资源不存在。
        FORBIDDEN_REQUEST, //表示客户端请求的资源被服务器拒绝访问。
        FILE_REQUEST, //表示客户端请求的资源可以被服务器处理。
        INTERNAL_ERROR, //表示服务器内部发生错误。
        CLOSED_CONNECTION //表示客户端与服务器之间的连接已关闭。
    };
    enum LINE_STATUS{
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
public:
    http_conn(){

    }
    ~http_conn(){

    }
public:
    //初始化连接
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    //关闭连接
    void close_conn(bool real_close = true);
    //处理请求
    void process();
    //从客户端读取数据
    bool read_once();
    //向客户端写入数据
    bool write();
    sockaddr_in *get_address(){
        return &m_address;
    }
    //初始化数据库连接池
    void initmysql_result(connection_pool *connPool);
    // 标志位，用于标识定时器是否超时。
    int timer_flag;
    // 一个标志位，可能用于记录某种改进的状态
    int improv;
private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text); //解析请求行。
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line(){
        return m_read_buf + m_start_line;
    }
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state; //读为0，写为1
private:
    int m_sockfd; //套接字文件描述符，用于标识连接的唯一标识符
    sockaddr_in m_address; //连接的地址信息，包括 IP 地址和端口号。
    char m_read_buf[READ_BUFFER_SIZE]; // 用于存储从客户端读取的数据的缓冲区。
    long m_read_idx; //当前已经读取的数据的索引。
    long m_checked_idx; //当前正在检查的数据的索引。
    int m_start_line; //当前正在分析的行的起始位置索引。
    char m_write_buf[WRITE_BUFFER_SIZE]; //用于存储向客户端写入的数据的缓冲区。
    int m_write_idx; //当前已经写入的数据的索引
    CHECK_STATE m_check_state; //当前 HTTP 请求的解析状态，包括解析请求行、头部和内容。
    METHOD m_method; //HTTP 请求方法，如 GET、POST 等。
    char m_real_file[FILENAME_LEN]; //实际请求的文件路径。
    char *m_url; //HTTP 请求中的 URL。
    char *m_version; //HTTP 协议的版本号。
    char *m_host; //HTTP 请求中的主机信息。
    long m_content_length; //HTTP 请求中的内容长度。
    bool m_linger; //HTTP 请求中的连接是否保持。
    char *m_file_address; //请求文件在内存中的映射地址。
    struct stat m_file_stat; //请求文件的状态信息。
    struct iovec m_iv[2]; //用于写入操作的 iovec 结构体数组。
    int m_iv_count; //iovec 结构体数组的长度。
    int cgi; //是否启用了 CGI（Common Gateway Interface），用于处理 POST 请求
    char *m_string; //用于存储请求头数据。
    int bytes_to_send; //需要发送的字节数。
    int bytes_have_send; //已经发送的字节数。
    char *doc_root; //服务器的根目录。

    map<string,string> m_users; //存储用户的用户名和密码的映射表。
    int m_TRIGMode; //触发模式，用于设置 ET 或 LT 模式。
    int m_close_log; //是否关闭日志记录。

    char sql_user[100]; //数据库用户名。
    char sql_passwd[100]; //数据库密码。
    char sql_name[100]; //数据库名称。

};


#endif //GUOWEBSERVER_HTTP_CONN_H
