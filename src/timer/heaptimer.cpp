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

typedef std::shared_ptr<std::function<void()>> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

auto startClock = Clock::now();

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
        printf("id = %i, count = %i, expire = %li \n", node.id_, node.count_,std::chrono::duration_cast<std::chrono::milliseconds>(node.expires_ - Clock::now()).count());
        if(it->second.avaliableCount_ != node.count_){
            // 延迟删除
            printf("delete id = %i\n", node.id_);
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
        res = std::chrono::duration_cast<std::chrono::milliseconds>(queue.top().expires_ - startClock).count();
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

int main() {
    HeapTimer timer;
    // 添加计时器
    timer.add(1, 3000, std::make_shared<std::function<void()>>([](){ std::cout << "Timer 1 expired\n"; }));
    timer.add(2, 2000, std::make_shared<std::function<void()>>([](){ std::cout << "Timer 2 expired\n"; }));
    
    // 触发计时
    timer.tick();
    
    // 重置计时器
    timer.reset(1, 1500);
    
    // 完成并触发回调
    timer.done(2);
    
    // 再次触发计时
    while (true)
    {
        //模拟休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timer.tick();
    }

     return 0;
}