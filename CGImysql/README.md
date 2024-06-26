
校验 & 数据库连接池
===============
数据库连接池
> * 单例模式，保证唯一
> * list实现连接池
> * 连接池为静态大小
> * 互斥锁实现线程安全

校验
> * HTTP请求采用POST方式
> * 登录用户名和密码校验
> * 用户注册及多线程注册安全


RAII 意思是 "Resource Acquisition Is Initialization"，这是一种 C++ 中的编程范式，也是一种重要的编程理念。

在 RAII 中，资源的获取（比如内存、文件描述符、锁等）与其生命周期的初始化绑定在一起。这意味着，当一个对象被创建时，它就会获取所需要的资源；当对象被销毁时，资源也会被释放。这样可以确保资源的正确分配和释放，从而避免内存泄漏和资源泄漏等问题。

RAII 的核心思想是利用对象的构造函数和析构函数来管理资源的生命周期。当对象被创建时，其构造函数会被调用，从而执行资源的获取操作；当对象被销毁时，其析构函数会被调用，从而执行资源的释放操作。这种方式使得资源的管理变得简单、可靠且安全。

例如，在 C++ 中，使用智能指针（如 `std::unique_ptr`、`std::shared_ptr`）可以很好地体现 RAII 的概念。智能指针在构造函数中获取资源（动态分配的内存），在析构函数中释放资源（自动调用 `delete`）。这样，即使在异常情况下，当对象被销毁时，资源也会被正确释放，从而避免了内存泄漏的风险。