#ifndef THREADPOOLH
#define THREADPOOLH
#include "../log/blockQueue.h" // Include BlockDeque
#include <functional>
#include <future>
#include <thread>
#include <vector>

using namespace std;
using Task = packaged_task<void()>; // 为了保证通用性，把所有任务都定义成返回void，参数void的packaged_task

class ThreadPool {
private:
    size_t threadNum_;
    BlockDeque<Task> taskQueue_; // Using BlockDeque instead of queue
    vector<thread> workers_;
    bool isClosed_;

public:
    ThreadPool()
        : threadNum_(thread::hardware_concurrency())
        , taskQueue_(1000) // Initialize BlockDeque with capacity
        , isClosed_(false) { };
    ~ThreadPool()
    {
        isClosed_ = true;
        taskQueue_.Close(); // Use BlockDeque's Close method
        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
        return;
    };
    template <class F, class... Args>
    auto commit(F&& f, Args&&... args) -> future<decltype(f(args...))>
    {
        using RetType = decltype(f(args...));
        auto task = make_shared<packaged_task<RetType()>>(bind(forward<F>(f), forward<Args>(args)...)); 
        // 通过bind消除参数，得到void ->RetType 的callable object
        
        // Use emplace_back instead of push_back with move
        // 使用lambda表达式消除出参
        taskQueue_.emplace_back([task]() { (*task)(); });
        
        future<RetType> res = task->get_future();
        return res;
    }
    void start()
    {
        LOG_INFO("ThreadPool start");
        for (size_t i = 0; i < threadNum_; i++) {
            workers_.emplace_back(
                thread([this]() {
                while (true)
                {   
                    // Use pop_move instead of optional-based pop
                    Task task;
                    bool success = taskQueue_.pop_move(task);
                    if(!success) { // If queue is closed or operation failed
                        return;
                    }
                    task();
                } }));
        }
    };
};

#endif

/*
## 线程池的设计
1. 实例化阻塞队列，得到任务队列
2. 将所有任务存储到任务队列中
2. 实例化若干个线程构成线程池，每个线程池上锁
3. 每个线程循环执行process函数
4. process函数：当队列非空，加锁，从队列中取出任务执行，返回

*/