#ifndef IPLISTH
#define IPLISTH
#include <string>       // 用于 std::string
#include <cstdio>       // 用于 FILE*, fopen, fclose, fseek, fputs, fflush, snprintf
#include <cstdint>      // 用于 uint32_t, uint16_t
#include <netinet/in.h> // 用于 struct sockaddr_in
#include <arpa/inet.h>  // 用于 ntohl, ntohs
#include <chrono>
#include <fmt/format.h>
using namespace std;

class iplist
{
private:
    string path_;
    FILE *file_;

public:
    iplist(const string &path) : path_(path), file_(nullptr)
    {
        file_ = fopen(path_.c_str(), "a+");
        if (file_)
        {
            fseek(file_, 0, SEEK_SET);
        }
    };
    ~iplist()
    {
        if (file_)
        {
            fclose(file_);
        }
    };

    void insert(struct sockaddr_in addr)
    {
        if (file_)
        {
            auto now = chrono::system_clock::now();
            time_t now_time_t = chrono::system_clock::to_time_t(now);
            tm t = *localtime(&now_time_t);
            string buf = (fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} {}.{}.{}.{}:{}\n",
                                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                                      t.tm_hour, t.tm_min, t.tm_sec,
                                      addr.sin_addr.s_addr & 0xFF, (addr.sin_addr.s_addr >> 8) & 0xFF, (addr.sin_addr.s_addr >> 16) & 0xFF, (addr.sin_addr.s_addr >> 24) & 0xFF,
                                      ntohs(addr.sin_port)));
            fputs(buf.c_str(), file_);
            fflush(file_);
        }
    };
};

#endif