#ifndef EPOLLER_H
#define EPOLLER_H
/*
设计思路：提供对epoll的封装，实现注册fd，修改，删除，查询事件
避免内核事件表的fd和socket的fd混用
避免直接暴露内核态接口
*/
#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool AddFd(int fd, uint32_t events);

    bool ModFd(int fd, uint32_t events);

    bool DelFd(int fd);

    int Wait(int timeoutMs = -1);  // 在timeout内更新events_

    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;

    std::vector<struct epoll_event> events_;    
};
#endif //EPOLLER_H