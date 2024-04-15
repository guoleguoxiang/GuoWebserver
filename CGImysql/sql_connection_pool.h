//
// Created by 19851 on 2024/3/23.
//

#ifndef GUOWEBSERVER_SQL_CONNECTION_POOL_H
#define GUOWEBSERVER_SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool{
public:
    MYSQL *GetConnection(); //获取数据库连接
    bool ReleaseConnection(MYSQL *con); //释放连接
    int GetFreeConn(); //获取连接
    void DestroyPool(); //销毁所有连接

    //单例模式
    static connection_pool *GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn; //最大连接数
    int m_CurConn; //当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数
    locker lock;
    list<MYSQL *> connList; //连接池
    sem reserve;

public:
    string m_url; //主机地址
    string m_Port; //数据库端口号
    string m_User; //登录数据库用户名
    string m_PassWord; //登录数据库密码
    string m_DatabaseName; //使用数据库名
    int m_close_log; //日志开关
};


// connectionRAII 类是一个辅助类，用于在获取数据库连接时自动管理连接的生命周期
/*
 * 通过使用 connectionRAII 类，可以在获取数据库连接时避免忘记释放连接而导致资源泄漏。
 * 它利用了 C++ 的对象生命周期管理机制，使得资源的获取和释放变得更加安全和方便。

     在使用时，可以将 connectionRAII 对象声明为局部变量，
     当对象超出作用域时，析构函数会自动调用，从而释放数据库连接，实现了资源的自动管理。
 *
 * */
class connectionRAII{
public:
    connectionRAII(MYSQL **con, connection_pool *connPool);
    ~connectionRAII();
private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};



#endif //GUOWEBSERVER_SQL_CONNECTION_POOL_H
