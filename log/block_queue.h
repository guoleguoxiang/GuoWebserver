//
// Created by 19851 on 2024/3/21.
//

#ifndef GUOWEBSERVER_BLOCK_QUEUE_H
#define GUOWEBSERVER_BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

template<class T>
class block_queue{
public:
    block_queue(int max_size = 1000){
        if(max_size<=0){
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    void clear(){
        m_mutex.lock();

        m_size = 0;
        m_front = -1;
        m_back = -1;

        m_mutex.unlock();
    }

    ~block_queue(){
        m_mutex.lock();
        if(m_array!=NULL){
            delete [] m_array;
        }
        m_mutex.unlock();
    }
    //判断队列是否满了
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //判断队列是否为空
    bool empty(){
        m_mutex.lock();
        if(0==m_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    //返回队首元素
    bool front(T &value){
        m_mutex.lock();
        if(0==m_size){
            m_mutex.unlock();
            return true;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    //返回队尾元素
    bool back(T &value){
        m_mutex.lock();
        if(0==m_size){
            m_mutex.unlock();
            return true;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }
    int size(){
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }
    int max_size(){
        int max_tmp =0;
        m_mutex.lock();
        max_tmp = m_max_size;
        m_mutex.unlock();
        return max_tmp;
    }
    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列,相当于生产者生产了一个元素
    //若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &item){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        //计算下一个要插入元素的位置，这里使用循环队列的方式来实现，确保队列的环形结构。
        m_back = (m_back+1)%m_max_size;
        m_array[m_back] = item;
        m_size++;
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }
    //pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T &item){
        m_mutex.lock();
        while(m_size<=0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front+1)%m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
    //增加了超时处理
    bool pop(T &item, int ms_timeout){
        struct timespec t = {0,0}; //用于设置超时时间。
        struct timeval now = {0,0}; //用于获取当前时间
        gettimeofday(&now, NULL); //使用 gettimeofday 函数获取当前时间，并将其存储在 now 变量中。
        m_mutex.lock();
        if(m_size<=0){
            t.tv_sec = now.tv_sec + ms_timeout/1000; //计算超时的秒数部分。
            t.tv_nsec = (ms_timeout%1000)*1000; //计算超时的纳秒数部分。
            if(!m_cond.timewait(m_mutex.get(),t)){
                m_mutex.unlock();
                return false;
            }
        }

        //如果队列仍为空（可能是超时引起的），则直接返回 false。
        if(m_size<=0){
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front+1)%m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
private:
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;

};

#endif //GUOWEBSERVER_BLOCK_QUEUE_H
