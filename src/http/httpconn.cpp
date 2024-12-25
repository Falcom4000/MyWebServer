#include "httpconn.h"
using namespace std;

string HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn()
{
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int sockFd, const sockaddr_in& addr){
    assert(sockFd > 0);
    userCount++;
    addr_ = addr;
    fd_ = sockFd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
}

ssize_t HttpConn::read(int* saveErrno){
    // 从sockFd读到ReadBuff
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_); // fd_非阻塞，能写多少写多少，如果内核缓冲区满了也不会等待
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(ToWriteBytes()  == 0) { break; } /* 都写完了 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            /* 如果iov_[1]被写了 说明iov[0] 已经写完了*/
            /*  是iov_[1]已经写入的长度* 往前移以适应*/
            size_t alreadyWrittenIov1 = len - iov_[0].iov_len;
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + alreadyWrittenIov1;
            iov_[1].iov_len -= alreadyWrittenIov1;
            if(iov_[0].iov_len) {
                /* 移动writebuff的读指针*/
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            /* iov[0] 还没写完 移动！*/
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240); // ET模式必须一口气读完
    return len;
}

bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(readBuff_)) {
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }

    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    return true;
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[{}]({}:{}) quit, UserCount:{}",
                 fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}