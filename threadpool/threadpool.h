//
// Created by 19851 on 2024/3/23.
//

#ifndef GUOWEBSERVER_THREADPOOL_H
#define GUOWEBSERVER_THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template<typename T>
class threadpool{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number=8, int max_request=10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);
private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();
private:
    int m_thread_number; //线程池中的线程数
    int m_max_requests; //请求队列中允许的最大请求数
    pthread_t *m_threads; //描述线程池的数组，其大小为m_thead_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker; //保护请求队列的互斥锁
    sem m_queuestat; //是否有任务需要处理
    connection_pool *m_connPool; //数据库
    int m_actor_model; //模型切换
};

template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests):m_actor_model(actor_model),m_thread_number(thread_number),m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool){
    if(thread_number<=0 || max_requests<=0){
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for(int i=0; i<thread_number; ++i){
        if(pthread_create(m_threads+i, NULL, worker, this) != 0){ //创建成功返回0，失败返回一个非零的错误码
            delete [] m_threads;
            throw std::exception();
        }
        // pthread_detach将指定的线程设置为分离状态，
        // 这意味着该线程结束时，其所占用的系统资源会自动释放，无需等待其他线程调用 pthread_join() 来获取其返回值。
        if(pthread_detach(m_threads[i])){ // 成功则返回 0，否则返回相应的错误码
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
}

template<typename T>
bool threadpool<T>::append(T *request, int state){
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

/*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
template<typename T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool*)arg;
    pool->run();
    return pool;
}
//于执行线程池中工作线程的实际工作
template<typename T>
void threadpool<T>::run(){
    while(true){
        m_queuestat.wait(); //等待任务队列非空，发信号
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue; //继续循环等待
        }
        T *request = m_workqueue.front(); //获取队首元素
        m_workqueue.pop_front(); //弹出队首元素
        m_queuelocker.unlock();

        if(!request){
            continue; //如果请求为空，继续下一次循环
        }
        // 根据不同的模型选择处理方式
        if(1==m_actor_model){
            if(0==request->m_state){ // 如果请求状态为读
                if(request->read_once()){ //读一次
                    request->improv = 1; // 标记为已处理
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }else{
                    request->improv = 1; // 标记为已处理
                    request->timer_flag = 1; // 设置定时器标志
                }
            }else{ // 如果请求状态为写
                if(request->write()){ // 执行写操作
                    request->improv = 1; // 标记为已处理
                }else{
                    request->improv = 1;  // 标记为已处理
                    request->timer_flag = 1;  // 设置定时器标志
                }
            }
        }else{ // 如果模型不为1
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }

    }//while
}

#endif //GUOWEBSERVER_THREADPOOL_H
