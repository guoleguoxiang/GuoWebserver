//
// Created by 19851 on 2024/3/21.
//

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log.h"

Log::Log(){
    m_count = 0;
    m_is_async = false;
}
Log::~Log(){
    if( m_fp != NULL ){
        fclose(m_fp);
    }
}

//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    //如果设置了max_queue_size,则设置为异步
    if(max_queue_size>=1){
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size); //用于将指定内存区域的内容设置为指定的值
    m_split_lines = split_lines;

    time_t t = time(NULL); // 获取当前时间，并将其保存在变量 t 中
    struct tm *sys_tm = localtime(&t); // localtime() 函数将时间转换为本地时间，并保存在结构体 tm 中
    struct tm my_tm = *sys_tm; // 将本地时间复制到另一个结构体 my_tm 中。

    // 用于在字符串中查找最后一个出现的指定字符，并返回该字符及其后面的部分。
    //这段代码通常用于从文件路径中提取文件名，即获取路径中最后一个 '/' 后面的部分，以便进行进一步处理。
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if(p==NULL){
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);
    }else{
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;
    // 在初始化日志模块时打开日志文件，并检查文件是否成功打开。如果文件打开失败，则初始化失败，返回 false；否则，返回 true，表示初始化成功。
    m_fp = fopen(log_full_name, "a");
    if(m_fp==NULL){
        return false;
    }

    return true;

}
//用于向日志中写入信息，支持格式化输出，类似于 printf 函数。
void Log::write_log(int level, const char *format, ...){
    struct timeval now = {0,0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level) {
        case 0:
            strcpy(s, "[debug]: "); // 用于将一个字符串复制到另一个字符串中
            break;
        case 1:
            strcpy(s, "[info]: ");
            break;
        case 2:
            strcpy(s, "[warn]: ");
            break;
        case 3:
            strcpy(s, "[erro]: ");
            break;
        default:
            strcpy(s, "[info]: ");
            break;
    }

    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    m_count++;
    //和当天日期不对应 或者 m_count=0
    if(m_today!=my_tm.tm_mday || m_count%m_split_lines == 0){
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year+1900, my_tm.tm_mon+1,my_tm.tm_mday);

        if(m_today != my_tm.tm_mday){
            snprintf(new_log, 255, "%s%s%s", dir_name,tail,log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name,tail,log_name,m_count/m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    // valst 用于存储 write_log 函数中传入的可变参数列表，其中 format 是格式化字符串，其后的参数是用于填充格式化字符串的实际参数。
    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = vsnprintf(m_buf+n, m_log_buf_size-n-1, format, valst);
    m_buf[n+m] = '\n';
    m_buf[n+m+1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }else{
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}
// 刷新日志，将缓冲区中的日志内容写入到日志文件中
void Log::flush(void ){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}

