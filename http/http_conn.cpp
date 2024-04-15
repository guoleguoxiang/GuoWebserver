//
// Created by 19851 on 2024/3/22.
//

#include "http_conn.h"
#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string,string> users;

//初始化数据库连接池
void http_conn::initmysql_result(connection_pool *connPool){
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if( mysql_query(mysql, "select username,passwd from user") ){ //执行成功，返回值为0；如果执行失败，返回值为非0
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *filelds = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while ( MYSQL_ROW row = mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

//对文件描述符设置非阻塞
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    // 触发模式，通常1为边缘触发模式 0为水平触发模式
    if(1==TRIGMode){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if(one_shot){
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
//从内核时间表删除描述符
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0); //从 epoll 内核事件表中删除指定的文件描述符
}
//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(1==TRIGMode){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close){
    if( real_close && (m_sockfd != -1) ){
        printf("close %d \n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, int close_log,
                     string user, string passwd, string sqlname){
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init(){
    mysql = NULL;
    bytes_to_send=0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag=0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK成功解析一行,LINE_BAD解析出的行内容有错误,LINE_OPEN当前行还没有解析完成
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for(; m_checked_idx<m_read_idx; ++m_checked_idx){
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r'){ // '\r'回车 '\n'换行 '\0'字符串结束的空字符
            if( (m_checked_idx + 1)==m_read_idx ){ //已经找到了'\r'，但是已经到了末尾，说明该行未结束
                return LINE_OPEN;
            } else if(m_read_buf[m_checked_idx+1]=='\n'){ //找到了'\r''\n'说明当前行已经结束
                m_read_buf[m_checked_idx++] = '\0'; // 将 \r\n 行结束符替换为 \0\0，以便后续的处理更方便。
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp=='\n'){
            if(m_checked_idx>1 && m_read_buf[m_checked_idx-1]=='\r'){
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }//for
    return LINE_OPEN;
}

//从客户端读取数据
//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
//读到的数据参到了m_read_buf缓冲区
bool http_conn::read_once(){
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;

    //LT读取数据  单次调用 recv() 函数读取数据
    if(0==m_TRIGMode){
        //m_read_buf+m_read_idx表示一个指针，指向读缓冲区 m_read_buf 中的位置，其位置为当前已经读取到的数据后面。
        // 这样做的目的是为了将新读取到的数据追加到已有数据的末尾处，从而实现数据的累积读取。
        bytes_read = recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
        m_read_idx += bytes_read;
        if(bytes_read<=0){
            return false;
        }
        return true;
    }else{ //ET读数据  需要一次性将数据读完
        while(true){
            bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
            if(bytes_read==-1){
                if(errno==EAGAIN || errno==EWOULDBLOCK){ // 这两个错误码表示暂时没有数据可读取
                    break;
                }
                return false;
            }else if(bytes_read==0){
                return false;
            }
            //正常读取
            m_read_idx += bytes_read;
        }//while
        return true;
    }//else
}

//解析http请求行，获得请求方法，目标url及http版本号
/*
 * http请求行
 * <Method> <Request-URI> <HTTP-Version>
 *       GET /index.html HTTP/1.1
 *  \t 是制表符（Tab）的转义字符
 *  制表符（Tab）是一种特殊的空白字符，用于在文本中产生一定数量的空格，通常用来进行对齐或者缩进。
 *  在大多数情况下，一个制表符通常等同于若干个空格字符的宽度。
 * */
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    //strpbrk() 函数用于在字符串中查找给定字符集合中的任意字符的第一个匹配项，并返回该字符在字符串中的位置（指针）
    m_url = strpbrk(text, " \t"); //定位 HTTP 请求行中请求方法结束的位置
    if(!m_url){
        return BAD_REQUEST;
    }
    //将 m_url 指向的位置的字符修改为 '\0'，即空字符，表示字符串的结束。然后通过 m_url++ 将指针移动到下一个位置，以便后续解析。
    *m_url++ = '\0'; //将制表符修改为 '\0' 可以将请求方法和 URL 分隔开来
    char *method = text;
    // strcasecmp 是一个不区分大小写比较字符串的函数，用于比较两个字符串是否相等
    if(strcasecmp(method, "GET") == 0){
        m_method = GET;
    }else if(strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }else{
        return BAD_REQUEST;
    }

    //size_t strspn(const char *str1, const char *str2);
    //strspn() 函数会在 str1 中从开头开始搜索，计算连续包含在 str2 中的字符的长度，并返回该长度
    //在这里，strspn(m_url, " \t") 返回了 m_url 指针所指向的字符串中以空格或制表符开头的字符的长度。
    // 然后 m_url 指针根据这个长度向后移动，指向第一个不是空格或制表符的字符。
    m_url += strspn(m_url, " \t");
    //strpbrk 函数在字符串中搜索第一次出现指定字符集合中任一字符的位置，并返回指向该位置的指针。
    //在这里，strpbrk(m_url, " \t") 就是在 m_url 指针所指向的字符串中搜索第一次出现空格或制表符的位置，并返回指向该位置的指针赋值给 m_version。
    m_version = strpbrk(m_url, " \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1")!= 0){
        return BAD_REQUEST;
    }
    //如果是以 "http://" 开头，那么将 m_url 指针移动 7 个位置，跳过 "http://" 部分，
    // 然后再使用 strchr 函数在剩余的字符串中查找第一个 '/' 字符的位置。这样做是为了获取主机名后的路径部分
    if( strncasecmp(m_url, "http://", 7)==0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if(strncasecmp(m_url, "https://", 8)==0){
        m_url+=8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0]!= '/'){
        return BAD_REQUEST;
    }
    //当url为/时，显示判断界面
    if(strlen(m_url)==1){
        strcat(m_url, "judge.html");
    }
    m_check_state = CHECK_STATE_HEADER;//即将解析http头部信息
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    if( text[0]=='\0'){ //检查文本是否为空，如果为空，则表示头部信息解析完毕
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT; //表示接下来需要处理请求体
            return NO_REQUEST; //表示解析完头部但请求未处理完成。
        }
        return GET_REQUEST;
    }else if(strncasecmp(text, "Connection:", 11)==0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive")==0){
            m_linger = true;
        }
    }else if(strncasecmp(text, "Content-length:", 15)==0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }else if(strncasecmp(text, "Host:", 5)==0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }else{
        LOG_INFO("oop! unknow header : %s", text);
    }
    return NO_REQUEST;
}


//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text){
    if( m_read_idx >= (m_content_length+m_checked_idx) ){
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST; //成功解析出完整的 HTTP 请求
    }
    return NO_REQUEST;
}

// 处理读取到的 HTTP 请求数据，解析请求行、请求头和请求体
http_conn::HTTP_CODE http_conn::process_read(){

    // 初始化行状态和返回状态
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    // 初始化文本指针
    char *text = 0;

    // 循环处理 HTTP 请求数据，直到请求解析完毕或者出现错误
    while( (m_check_state==CHECK_STATE_CONTENT && line_status==LINE_OK) ||
            ( (line_status=parse_line()) == LINE_OK ) )  {
        text = get_line();// 获取一行 HTTP 数据
        m_start_line = m_checked_idx;// 记录当前行的起始索引
        LOG_INFO("%s", text);// 打印日志，显示当前行内容
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }else if(ret == GET_REQUEST){ //如果解析结果为 GET_REQUEST，则执行请求处理并返回
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret==GET_REQUEST){
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }//while
    return NO_REQUEST; // 返回无请求状态，表示继续读取数据
}

http_conn::HTTP_CODE http_conn::do_request(){
    // 将实际文件路径设置为服务器的根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    const char *p = strrchr(m_url, '/');

    if( cgi==1 && (*(p+1)=='2' || *(p+1)=='3') ){
        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char*) malloc(sizeof(char)*200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url+2);
        strncpy(m_real_file+len, m_url_real,FILENAME_LEN-len-1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for(i=5; m_string[i]!= '&'; ++i){
            name[i-5] = m_string[i];
        }
        name[i-5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        // 2登录 3注册
        if( *(p+1)=='3' ){ //注册
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            // sql_insert = INSERT INTO user(username, passwd) VALUES(name,passwd)
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'"); //strcat拼接字符串
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if(users.find(name) == users.end()){ //没有重名的
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string,string>(name,password));
                m_lock.unlock();

                if(!res){ //执行成功返回0
                    strcpy(m_url, "/log.html");
                }else{
                    strcpy(m_url, "/registerError.html");
                }
            }else{ //有重名的
                strcpy(m_url, "/registerError.html");
            }
        }else if( *(p+1)=='2'){ //如果是登录，直接判断
                                //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            if( users.find(name)!=users.end() && users[name]==password){
                strcpy(m_url, "/welcome.html");
            }else{
                strcpy(m_url, "/logError.html");
            }
        }

    }

    if( *(p+1)=='0' ){
        char *m_url_real = (char*) malloc(sizeof(char)*200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file+len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if( *(p+1)=='1' ){
        char *m_url_real = (char*) malloc(sizeof(char)*200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file+len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if( *(p+1)=='5' ){
        char *m_url_real = (char*) malloc(sizeof(char)*200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file+len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if( *(p+1)=='6'){
        char *m_url_real = (char*) malloc(sizeof(char)*200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file+len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else if( *(p+1)=='7'){
        char *m_url_real = (char*) malloc(sizeof(char)*200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file+len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }else{
        strncpy(m_real_file+len, m_url, FILENAME_LEN-len-1);
    }


    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;

}

//munmap() 函数是用于取消映射已映射内存区域的系统调用（System Call）
void http_conn::unmap(){
    munmap(m_file_address, m_file_stat.st_size);
    m_file_address= 0;
}

//向客户端写入数据
bool http_conn::write(){
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

// 用于向写缓冲区中添加HTTP响应内容
bool http_conn::add_response(const char *format, ...){
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}


bool http_conn::add_headers(int content_len){
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content){
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else
            {
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string))
                    return false;
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
//处理请求
void http_conn::process(){
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}