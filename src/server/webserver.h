#ifndef WEBSERVER_H
#define WEBSERVER_H
/*
设计思路
1.  <fd, HttpConn> 每个连接对应一个fd，每个fd对应一个HttpConn
2.  注册好内核事件表
3. 启动监听，将listenFd注册到内核事件表中
4. 使用epoller，循环等待事件
4.1 如果是listenfd有事件，accept一个fd，然后调用HttpConn.init，将其进行初始化
4.2 EPOLLIN事件，说明数据可读，从fd中读数据，读到HttpConn对应的ReadBuff_中
4.2.1 然后调用HttpConn.Process，处理请求，将返回值写入WriteBuff_里
4.2.2 最后，把fd在内核事件表里重新注册成EPOLLOUT，等待EPOLLOUT事件
4.3 EPOLLOUT事件，说明数据可写，往fd里写数据——聚集写，写http头和返回文件
4.3.1 写完后，把fd在内核事件表里重新注册成EPOLLIN，等待EPOLLIN事件




*/
#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../ThreadPool/ThreadPool.cpp"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void Start();

private:
    bool InitSocket_(); 
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);
  
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);   

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    char* srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
   
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};


#endif //WEBSERVER_H