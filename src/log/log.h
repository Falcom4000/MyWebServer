#ifndef LOG_H
#define LOG_H
/*
多线程异步日志
*/
#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h> // vastart va_end
#include <assert.h>
#include <sys/stat.h> //mkdir
#include "blockQueue.h"
#include <iomanip>
#include <fmt/core.h>
#include "../buffer/buffer.h"
using namespace std;
class Log
{
public:
    void init(int level, const char *path = "./log",
              const char *suffix = ".log",
              int maxQueueCapacity = 1024);

    static Log *Instance();
    static void FlushLogThread();
    template <typename... Args>
    void write(int level, string &&format, Args &&...args)
    {
        auto now = chrono::system_clock::now();
        time_t now_time_t = chrono::system_clock::to_time_t(now);
        tm t = *localtime(&now_time_t);

        /* 日志日期 日志行数 */
        if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0)))
        {
            unique_lock<mutex> locker(mtx_);
            locker.unlock();
            string newFile;
            string tail = fmt::format("{:04}_{:02}_{:02}", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
            if (toDay_ != t.tm_mday)
            {
                newFile = fmt::format("{}/{}{}", path_, tail, suffix_);
                toDay_ = t.tm_mday;
                lineCount_ = 0;
            }
            else
            {
                newFile = fmt::format("{}/{}-{}{}", path_, tail, (lineCount_ / MAX_LINES), suffix_);
            }

            locker.lock();
            flush();
            fclose(fp_);
            fp_ = fopen(newFile.c_str(), "a");
            assert(fp_ != nullptr);
        }

        {
            unique_lock<mutex> locker(mtx_);
            lineCount_++;
            buff_.Append(fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} ",
                                     t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                                     t.tm_hour, t.tm_min, t.tm_sec));
            AppendLogLevelTitle_(level);
            string message = fmt::vformat(format, fmt::make_format_args(forward<Args>(args)...));
            buff_.Append(message.c_str(), message.size());
            buff_.Append("\n\0", 2);
            if (deque_ && !deque_->full())
            {
                deque_->push_back(buff_.RetrieveAllToStr());
            }
            else
            {
                fputs(buff_.Peek(), fp_);
            }

            buff_.RetrieveAll();
        }
    }
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }

private:
    Log();
    void AppendLogLevelTitle_(int level);
    ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char *path_;
    const char *suffix_;

    int lineCount_;
    int toDay_;

    bool isOpen_;

    Buffer buff_;
    int level_;

    FILE *fp_;
    unique_ptr<BlockDeque<string>> deque_;
    unique_ptr<thread> writeThread_;
    mutex mtx_;
};

#define LOG_BASE(level, format, ...)                          \
    do                                                        \
    {                                                         \
        Log *log = Log::Instance();                           \
        if (log->IsOpen() && log->GetLevel() <= level)        \
        {                                                     \
            log->write(level, string(format), ##__VA_ARGS__); \
            log->flush();                                     \
        }                                                     \
    } while (0);

#define LOG_DEBUG(format, ...)             \
    do                                     \
    {                                      \
        LOG_BASE(0, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_INFO(format, ...)              \
    do                                     \
    {                                      \
        LOG_BASE(1, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_WARN(format, ...)              \
    do                                     \
    {                                      \
        LOG_BASE(2, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_ERROR(format, ...)             \
    do                                     \
    {                                      \
        LOG_BASE(3, format, ##__VA_ARGS__) \
    } while (0);

#endif // LOG_H