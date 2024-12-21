#ifndef HEAPTIMERH
#define HEAPTIMERH

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include <memory>
#include <thread>

typedef std::shared_ptr<std::function<void()>> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode
{
    int id_; // id = fd_
    TimeStamp expires_;
    TimeoutCallBack cb_;
    int count_;
    int avaliableCount_;
    bool operator<(const TimerNode &other) const
    {
        return expires_ > other.expires_; // 最早到期的定时器优先
    }

    TimerNode(int id, TimeStamp expires,
              TimeoutCallBack cb, int count, int avaliableCount)
        : id_(id), expires_(expires), cb_(cb), count_(count), avaliableCount_(avaliableCount) {}
    TimerNode() 
        : id_(0), expires_(Clock::now()), cb_(nullptr), count_(0), avaliableCount_(-1) {}
};

struct TimerCmp
{
    bool operator()(const TimerNode &a, const TimerNode &b) const {
        return a.expires_ > b.expires_;
    }
};
class HeapTimer
{
private:
    std::priority_queue<TimerNode, std::vector<TimerNode>, TimerCmp> queue;
    std::unordered_map<int, TimerNode> map; // fd - > timernode

public:
    ~HeapTimer();
    void reset(int id, int newExpires);

    void add(int id, int timeOut, TimeoutCallBack cb);

    void done(int id);

    void clear();

    void tick();

    void pop();

    void del(int id);

    int GetNextTimeout();
};

#endif