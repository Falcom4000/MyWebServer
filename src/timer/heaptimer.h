#ifndef HEAPTIMERH
#define HEAPTIMERH

#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <thread>
#include <time.h>
#include <unordered_map>

typedef std::shared_ptr<std::function<void()>> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id_; // id = fd_
    TimeStamp expires_;
    TimeoutCallBack cb_;
    
    bool operator<(const TimerNode& other) const
    {
        return expires_ < other.expires_ || (expires_ == other.expires_ && id_ < other.id_);
    }

    TimerNode(int id, TimeStamp expires, TimeoutCallBack cb)
        : id_(id)
        , expires_(expires)
        , cb_(cb)
    {
    }
    
    TimerNode()
        : id_(0)
        , expires_(Clock::now())
        , cb_(nullptr)
    {
    }
};

class HeapTimer {
private:
    std::set<std::shared_ptr<TimerNode>> timers_;
    std::unordered_map<int, std::weak_ptr<TimerNode>> map_; // fd -> weak_ptr to timernode

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