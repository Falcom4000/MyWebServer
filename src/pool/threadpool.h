#ifndef THREADPOOLH
#define THREADPOOLH
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>

using namespace std;
using Task = packaged_task<void()>; // 为了保证通用性，把所有任务都定义成返回void，参数void的packaged_task

class ThreadPool {
private:
    size_t threadNum_;
    queue<Task> taskQueue_;
    condition_variable cv_;
    vector<thread> workers_;
    bool isClosed_;
    mutex mtx_;

public:
    ThreadPool()
        : threadNum_(thread::hardware_concurrency())
        , isClosed_(false) { };
    ~ThreadPool()
    {
        isClosed_ = true;
        cv_.notify_all();
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
        auto task = make_shared<packaged_task<RetType()>>(
            bind(forward<F>(f), forward<Args>(args)...)); // 通过bind消除参数，得到task，void ->RetType 的callable object
        {
            auto lk = unique_lock(this->mtx_);
            this->taskQueue_.emplace(move([task]() { (*task)(); })); // lambda表达式为void - >void 的 callable object，可以用来构造Task
            this->cv_.notify_one();
        }
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
                    Task task;
                    {
                        auto lk = unique_lock<mutex>(this->mtx_); // 获得锁，尝试加锁
                        this->cv_.wait(lk, [this]{return this->isClosed_ || !this->taskQueue_.empty();}); // 如果关了或者任务队列里有任务，
                        if(this->isClosed_){return;}
                        task = move(this->taskQueue_.front());
                        this->taskQueue_.pop(); // 退出作用域后重新释放锁
                    }
                    task();
                } }));
        }
    };
};

#endif

/*
## 线程池的设计

### 成员
1. 任务队列 ''
2.

1. 实例化阻塞队列，得到任务队列
2. 将所有任务存储到任务队列中
2. 实例化若干个线程构成线程池，每个线程池上锁
3. 每个线程循环执行process函数
4. process函数：当队列非空，加锁，从队列中取出任务执行，返回





*/