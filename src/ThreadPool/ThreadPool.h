#include<queue>
#include<thread>
#include<condition_variable>
#include<functional>
#include<vector>
#include<future>


using namespace std;
using Task = packaged_task<void()>;

class ThreadPool
{
private:
    
    size_t threadNum_;
    queue<Task> taskQueue_;
    condition_variable cv_;
    vector<thread> workers_;
    bool isClosed_;
    mutex mtx_;

public:
    
        ThreadPool();
        ~ThreadPool();
        template<class F, class... Args>
        auto commit(F&& f, Args&&... args)-> future<decltype(f(args...))>;
        void start();
        
};