
线程同步机制包装类
===============
多线程同步，确保任一时刻只能有一个线程能进入关键代码段.
> * 信号量
> * 互斥锁
> * 条件变量



sem_t 是 POSIX 线程库中用于表示信号量的数据类型。
信号量是通过 sem_t 数据类型来表示的，程序通过调用 sem_init()、sem_wait()、sem_post() 等函数来操作信号量。 具体地，sem_t 可以看作是一个抽象的整数值，表示信号量的当前值，程序通过对这个值进行增减来实现信号量的控制。

sem_init()、sem_wait()、sem_post()这三个函数是 POSIX 线程库中用于操作信号量的函数，分别用于初始化信号量、等待（阻塞）信号量和增加信号量的值。

`int sem_init(sem_t *sem, int pshared, unsigned int value);`
sem_init() 函数用于初始化一个信号量。
sem 是指向要初始化的信号量的指针。
pshared 参数指定了信号量的共享性质，取值为 0 表示信号量只能在进程内部共享，取值为 1 表示信号量可以在多个进程之间共享。
value 参数指定了信号量的初始值，即信号量的初始计数器值。

`int sem_wait(sem_t *sem);`
sem_wait() 函数用于等待（阻塞）一个信号量。
当调用 sem_wait() 时，如果信号量的值大于 0，则将信号量的值减 1 并立即返回，表示成功获取了信号量；如果信号量的值为 0，则函数会阻塞当前线程，直到有其他线程调用 sem_post() 来增加信号量的值为止。

`int sem_post(sem_t *sem);`
sem_post() 函数用于增加一个信号量的值。
当调用 sem_post() 时，它会将信号量的值加 1。
如果有其他线程在等待这个信号量，调用 sem_post() 会唤醒其中一个等待线程；如果没有线程等待这个信号量，它仅仅是简单地增加了信号量的值。



pthread_mutex_t 是 POSIX 线程库中的一种类型，用于创建互斥锁（mutex）。互斥锁是一种线程同步机制，用于在多线程程序中保护共享资源免受并发访问的影响。

以下是 pthread_mutex_t 的基本操作函数：
- `pthread_mutex_init:` 用于初始化互斥锁。
- `pthread_mutex_destroy:` 用于销毁互斥锁。
- `pthread_mutex_lock:` 用于加锁。
- `pthread_mutex_unlock:` 用于解锁。


`pthread_mutex_init：`
用于初始化互斥锁。
函数原型：`int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)`;
参数 mutex 是一个指向互斥锁变量的指针，attr 是一个指向互斥锁属性的指针（通常设置为 NULL，表示使用默认属性）。
初始化互斥锁后，需要在使用前进行加锁操作。

`pthread_mutex_destroy：`
用于销毁互斥锁。
函数原型：`int pthread_mutex_destroy(pthread_mutex_t *mutex);`
参数 mutex 是一个指向待销毁互斥锁变量的指针。
在不再需要使用互斥锁时调用此函数，用于释放相关资源。

`pthread_mutex_lock：`
用于加锁。
函数原型：`int pthread_mutex_lock(pthread_mutex_t *mutex);`
参数 mutex 是一个指向互斥锁变量的指针。
如果互斥锁已被其他线程占用，则调用线程将被阻塞，直到获取到互斥锁为止。

`pthread_mutex_unlock：`
用于解锁。
函数原型：`int pthread_mutex_unlock(pthread_mutex_t *mutex);`
参数 mutex 是一个指向互斥锁变量的指针。
解锁操作将释放互斥锁，允许其他线程获取并使用共享资源。





`timespec` 是 POSIX 标准中定义的一个数据结构，用于表示时间的结构体。它通常用于在编程中表示精确的时间间隔或时间点。

`timespec` 结构体的定义如下：

```c
struct timespec {
    time_t tv_sec;  // 秒
    long tv_nsec;   // 纳秒
};
```

其中，`tv_sec` 字段表示时间的秒部分，`tv_nsec` 字段表示时间的纳秒部分。通过这两个字段的组合，`timespec` 结构体可以表示精确到纳秒的时间。

在 POSIX 环境中，`timespec` 结构体通常用于与时间相关的系统调用和函数，比如计时器、定时器、等待一段时间、获取当前时间等。一些 POSIX 标准库中的函数，比如 `nanosleep()`、`clock_gettime()` 等，接受或返回 `timespec` 结构体以实现对时间的操作。



`pthread_cond_t` 是 POSIX 线程库中用于条件变量的类型，它用于实现线程间的条件等待和通知。条件变量允许一个线程等待某个条件的发生，而其他线程在满足条件时发出通知，唤醒等待的线程。

以下是与 `pthread_cond_t` 相关的几个常用函数：

1. **pthread_cond_init**：
    - 用于初始化条件变量。
    - 函数原型：`int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);`
    - 参数 `cond` 是一个指向条件变量变量的指针，`attr` 是一个指向条件变量属性的指针（通常设置为 `NULL`，表示使用默认属性）。

2. **pthread_cond_destroy**：
    - 用于销毁条件变量。
    - 函数原型：`int pthread_cond_destroy(pthread_cond_t *cond);`
    - 参数 `cond` 是一个指向待销毁条件变量的指针。

3. **pthread_cond_wait**：
    - 用于等待条件变量满足。
    - 函数原型：`int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);`
    - 参数 `cond` 是一个指向条件变量的指针，`mutex` 是一个指向互斥锁的指针。调用该函数会释放 `mutex` 并阻塞当前线程，直到收到信号通知。

4. **pthread_cond_timedwait**：
    - 与 `pthread_cond_wait` 类似，但在超时前等待条件变量满足。
    - 函数原型：`int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);`
    - 参数 `abstime` 是一个指向等待超时时间的指针。如果在超时之前未收到信号通知，则该函数返回 `ETIMEDOUT`。

5. **pthread_cond_signal**：
    - 用于发送信号通知一个等待在条件变量上的线程。
    - 函数原型：`int pthread_cond_signal(pthread_cond_t *cond);`
    - 参数 `cond` 是一个指向条件变量的指针。调用该函数会唤醒至少一个等待在条件变量上的线程。

6. **pthread_cond_broadcast**：
    - 用于广播信号通知所有等待在条件变量上的线程。
    - 函数原型：`int pthread_cond_broadcast(pthread_cond_t *cond);`
    - 参数 `cond` 是一个指向条件变量的指针。调用该函数会唤醒所有等待在条件变量上的线程。

这些函数结合互斥锁的使用，可以实现多线程之间的条件等待和通知，帮助线程进行同步操作。条件变量常用于实现生产者-消费者模式、线程池等多线程编程场景。






