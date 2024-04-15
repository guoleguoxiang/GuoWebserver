//
// Created by 19851 on 2024/3/21.
//

#ifndef GUOWEBSERVER_LOG_H
#define GUOWEBSERVER_LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>         //standard arguments" 的缩写, 用于在函数中处理可变数量的参数
#include <pthread.h>       //#include<thread.h>已经被取代，不推荐使用
#include "block_queue.h"


using namespace std;

class Log {

public:
    //C++11以后,使用局部变量懒汉不用加锁
    // 用于获取日志记录器的单例实例
    static Log *get_instance(){
        static Log instance;
        return &instance;
    }
    // 用于在线程中刷新日志内容
    static void *flush_log_thread(void *args){
        Log::get_instance()->async_write_log();
    }
    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size=8192, int split_lines=5000000, int max_queue_size = 0);
    //用于向日志中写入信息，支持格式化输出，类似于 printf 函数。
    void write_log(int level, const char *format, ...);
    // 刷新日志，将缓冲区中的日志内容写入到日志文件中
    void flush(void );

private:
    Log();
    virtual ~Log();
    // 在异步模式下，用于异步写入日志
    void *async_write_log(){
        string single_log;
        // 从阻塞队列中取出一个日志string，写入文件
        while( m_log_queue->pop(single_log)){
            m_mutex.lock();
            // 用于将字符串single_log.c_str()写入到指定的文件流m_fp中
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines; //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count; //日志行数记录
    int m_today; //因为按天分类，记录当前时间是哪一天
    //FILE 结构体用于表示一个文件流（File Stream），它包含了执行文件输入输出操作所需的信息，比如文件描述符、文件位置指针、文件状态等。
    // 通过 FILE 结构体，C 程序可以打开文件、读取文件内容、写入文件内容、关闭文件等操作。
    FILE *m_fp; //打开log的文件指针
    char *m_buf; //日志缓冲区指针
    block_queue<string> *m_log_queue; //阻塞队列
    bool m_is_async; //是否同步标志位
    locker m_mutex;
    int m_close_log; //关闭日志


};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif //GUOWEBSERVER_LOG_H
