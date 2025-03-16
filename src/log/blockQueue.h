#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <condition_variable>
#include <deque>
#include <mutex>
#include <sys/time.h>
/*
有锁的线程安全队列
支持多生产者多消费者
*/
template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

    // Add methods for move semantics
    void push_back(T &&item);  // Move version of push_back
    void push_front(T &&item); // Move version of push_front
    template<typename... Args>
    void emplace_back(Args&&... args); // Emplace back to construct in-place
    
    // Replace std::optional methods with alternative move-based methods
    bool pop_move(T &item); // Pop that moves the value into item
    bool pop_move(T &item, int timeout); // Pop with timeout that moves the value

private:
    std::deque<T> deq_;

    size_t capacity_;

    std::mutex mtx_;

    bool isClose_;

    std::condition_variable condConsumer_;

    std::condition_variable condProducer_;
};


template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

template<class T>
void BlockDeque<T>::Close() {
    {   
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condProducer_.wait(locker, [&]{ return deq_.size() < capacity_; });
    deq_.push_back(item);
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condProducer_.wait(locker, [&]{ return deq_.size() < capacity_; });
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_back(T &&item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condProducer_.wait(locker, [&]{ return deq_.size() < capacity_; });
    deq_.push_back(std::move(item));
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(T &&item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condProducer_.wait(locker, [&]{ return deq_.size() < capacity_; });
    deq_.push_front(std::move(item));
    condConsumer_.notify_one();
}

template<class T>
template<typename... Args>
void BlockDeque<T>::emplace_back(Args&&... args) {
    std::unique_lock<std::mutex> locker(mtx_);
    condProducer_.wait(locker, [&]{ return deq_.size() < capacity_; });
    deq_.emplace_back(std::forward<Args>(args)...);
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condConsumer_.wait(locker, [&]{return !deq_.empty() || isClose_;});
    if(isClose_){
        return false;
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    bool success = condConsumer_.wait_for(locker, std::chrono::seconds(timeout),
    [&]{ return !deq_.empty() || isClose_; });
    if(isClose_ || !success) {
        return false;
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop_move(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    condConsumer_.wait(locker, [&]{return !deq_.empty() || isClose_;});
    if(isClose_ || deq_.empty()){
        return false;
    }
    item = std::move(deq_.front());
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop_move(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    bool success = condConsumer_.wait_for(locker, std::chrono::seconds(timeout),
    [&]{ return !deq_.empty() || isClose_; });
    if(isClose_ || !success || deq_.empty()) {
        return false;
    }
    item = std::move(deq_.front());
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H