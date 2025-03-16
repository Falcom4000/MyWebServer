#include "heaptimer.h"
#include "../log/log.h"
#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <chrono>
#include <functional>
#include <iostream>
#include <set>
#include <thread>
#include <time.h>
#include <unordered_map>

void HeapTimer::add(int id, int timeOut, TimeoutCallBack cb)
{
    // Create a new timer node
    auto timer = std::make_shared<TimerNode>(id, Clock::now() + MS(timeOut), cb);
    
    // Add to set
    timers_.insert(timer);
    
    // Update map with weak_ptr to the timer
    map_[id] = timer;
}

void HeapTimer::del(int id)
{
    // Find the timer in the map
    auto it = map_.find(id);
    if (it != map_.end()) {
        // Try to lock the weak_ptr
        if (auto timer = it->second.lock()) {
            // Remove from set if we can get a valid shared_ptr
            timers_.erase(timer);
        }
        // Remove from map
        map_.erase(it);
    }
}

void HeapTimer::reset(int id, int newExpires)
{
    // Find the timer in the map
    auto it = map_.find(id);
    if (it != map_.end()) {
        // If we can lock the weak_ptr, update its expiration time
        if (auto timer = it->second.lock()) {
            // Remove from set first since changing expires_ affects ordering
            timers_.erase(timer);
            
            // Update expiration time
            timer->expires_ = Clock::now() + MS(newExpires);
            
            // Re-insert with updated expiration time
            timers_.insert(timer);
        }
    }
}

void HeapTimer::done(int id)
{
    // Find the timer in the map
    auto it = map_.find(id);
    if (it != map_.end()) {
        // If we can lock the weak_ptr, execute callback and remove
        if (auto timer = it->second.lock()) {
            timer->cb_->operator()();
            timers_.erase(timer);
        }
        // Remove from map
        map_.erase(it);
    }
}

void HeapTimer::tick()
{
    auto now = Clock::now();
    
    // Process all expired timers
    while (!timers_.empty()) {
        auto it = timers_.begin();
        auto timer = *it;
        
        // If timer isn't expired yet, break
        if (timer->expires_ > now) {
            break;
        }
        
        // Execute callback
        timer->cb_->operator()();
        
        // Remove from set and map
        timers_.erase(it);
        map_.erase(timer->id_);
    }
}

int HeapTimer::GetNextTimeout()
{
    tick();
    int res = -1;
    
    if (!timers_.empty()) {
        auto timer = *timers_.begin();
        res = std::chrono::duration_cast<std::chrono::milliseconds>(
            timer->expires_ - Clock::now()).count();
        if (res < 0) {
            res = 0;
        }
    }
    return res;
}

void HeapTimer::clear()
{
    timers_.clear();
    map_.clear();
}

HeapTimer::~HeapTimer()
{
    clear();
}

// pop is not needed with set implementation but kept for API compatibility
void HeapTimer::pop()
{
    if (!timers_.empty()) {
        auto timer = *timers_.begin();
        timers_.erase(timers_.begin());
        map_.erase(timer->id_);
    }
}