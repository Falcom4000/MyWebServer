#include "log.h"
using namespace std;

Log::Log()
{
    lineCount_ = 0;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}
Log::~Log()
{
    if (writeThread_ && writeThread_->joinable())
    {
        while (!deque_->empty())
        {
            deque_->flush();
        };
        deque_->Close();
        writeThread_->join();
    }
    if (fp_)
    {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
    isOpen_ = false;
}
void Log::AppendLogLevelTitle_(int level)
{
    switch (level)
    {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}
void Log::flush()
{
    deque_->flush();
    fflush(fp_);
}
Log *Log::Instance()
{
    static Log log;
    return &log;
}

void Log::FlushLogThread()
{
    Log::Instance()->AsyncWrite_();
}

void Log::AsyncWrite_()
{
    string str = "";
    while (deque_->pop(str))
    {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

int Log::GetLevel()
{
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level)
{
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

void Log::init(int level, const char *path, const char *suffix, int maxQueueCapacity)
{
    /* 初始化各个成员变量 */
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    isOpen_ = true;
    deque_ = std::make_unique<BlockDeque<std::string>>(maxQueueCapacity);
    writeThread_ = std::make_unique<std::thread>(FlushLogThread);
    /* 先用时间格式化文件名 */
    auto now = chrono::system_clock::now();
    time_t now_time_t = chrono::system_clock::to_time_t(now);
    tm tm = *localtime(&now_time_t);
    string fileName = (fmt::format("{}/{:04}_{:02}_{:02}{}", path_, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, suffix_));
    /* 打开文件 */
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if (fp_)
        {
            deque_->flush();
            fflush(fp_);
            fclose(fp_);
        }
        fp_ = fopen(fileName.c_str(), "a");
        if (fp_ == nullptr)
        {
            mkdir(path_, 0777);
            fp_ = fopen(fileName.c_str(), "a");
        }
        assert(fp_ != nullptr);
    }
}