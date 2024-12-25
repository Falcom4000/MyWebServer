#include "sqlconnpool.h"
using namespace std;

SqlConnPool *SqlConnPool::Instance()
{
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char *host, int port,
                       const char *user, const char *pwd, const char *dbName,
                       int connSize = 10)
{
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++)
    {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql)
        {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql)
        {
            LOG_ERROR("MySql Connect error:{} ", mysql_error(sql));
        }
        connQue_.push_back(sql);
    }
    MAX_CONN_ = connSize;
}

MYSQL *SqlConnPool::GetConn()
{
    MYSQL *sql = nullptr;
    connQue_.pop(sql);
    return sql;
}

void SqlConnPool::FreeConn(MYSQL *sql)
{
    assert(sql);
    connQue_.push_back(sql);
}

void SqlConnPool::ClosePool()
{
    while (!connQue_.empty())
    {
        auto sql = GetConn();
        mysql_close(sql);
    }
    connQue_.Close();
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount()
{
    return connQue_.size();
}

SqlConnPool::~SqlConnPool()
{
    ClosePool();
}
