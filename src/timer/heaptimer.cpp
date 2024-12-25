#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include <iostream>
#include <thread>
#include "heaptimer.h"
#include "../log/log.h"
typedef std::shared_ptr<std::function<void()>> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

void HeapTimer::add(int id, int timeOut, TimeoutCallBack cb){
    // 确保只有最新添加进来的定时器才会被执行
    auto old = map.find(id);
    if (old == map.end())
    {
        TimerNode node(id, Clock::now() + MS(timeOut), cb, 1, 1);
        map[id] = node;
        queue.emplace(node);
    }
    else
    {
        TimerNode node(id, Clock::now() + MS(timeOut), cb, old->second.count_ + 1, old->second.count_ + 1);
        map[id] = node;
        queue.emplace(node);
    }  
}

void HeapTimer::del(int id){
    // 删除同一id所有的定时器，延迟删除map里的定时器
    map[id].count_ = 0;
    map[id].avaliableCount_ = -1;
}

void HeapTimer::reset(int id , int newExpires){
    // 添加一个新的定时器，旧的定时器都不会被触发
    add(id, newExpires, map[id].cb_);
}

void HeapTimer::done(int id){
    // 立刻完成一个定时器，并且删除剩下所有的定时器，延迟删除map里的定时器
    map[id].cb_->operator()();
    del(id);
}

void HeapTimer::tick(){
    while(!queue.empty()){
        auto node = queue.top();
        auto it = map.find(node.id_);
        std::chrono::duration_cast<std::chrono::milliseconds>(node.expires_ - Clock::now()).count();
        if(it->second.avaliableCount_ != node.count_){
            // 延迟删除
            it->second.count_--;
            queue.pop();
            if(it->second.count_ == 0){
                map.erase(it);
            }
        }
        else if(node.expires_ > Clock::now()){
            break;
        }
        else{
            it->second.cb_->operator()();
            it->second.count_--;
            queue.pop();
            if(it->second.count_ == 0){
                map.erase(it);
            }
        }
    }
    return;
}

int HeapTimer::GetNextTimeout() {
    tick();
    size_t res = -1;
    if(!queue.empty()) {
        res = std::chrono::duration_cast<std::chrono::milliseconds>(queue.top().expires_ - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}

void HeapTimer::clear()
    {
        while (!queue.empty())
        {
            queue.pop();
        }
        
        map.clear();
    }

HeapTimer::~HeapTimer()
{
    clear();
}