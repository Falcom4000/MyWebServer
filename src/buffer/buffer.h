#ifndef BUFFER_H
#define BUFFER_H

/*
设计思路：
连续地址vector<char>存放数据
readPos_和writePos_标记已读和未读数据的边界，原子变量
必须有readPos_ >= 0, writePos_ >= readPos_
0 - readPos_ 之间是已读数据
readPos_ - writePos_ 之间是已写入未读数据
writePos_ - buffer.size() 之间是预留空间

写策略：
1. 保证有足够的空间
1.1 如果剩余空间不够，且预留空间 + 可写空间 < 待写入数据的长度，resize
1.2 如果剩余空间不够，但预留空间 + 可写空间 >= 待写入数据的长度，移动数据
1.2.1 具体来说，将已写入未读数据移动到buffer的起始位置, readPos_ = 0, writePos_ = readPos_ + 已写入未读数据长度
1.2.2 std::copy(first, last, result) 把[first, last)地址复制到result后

读策略：
2. 全部读出到string中
2.1 len =  writePos_ - readPos_
2.2 string str(Peek(), len) 从readPos_开始的len长度的数据复制到str中
2.3 读出后，readPos_ = 0, writePos_ = 0
2.4 bzero(&buffer_[0], buffer_.size());

读文件策略
3.分散读 定义两个iovec结构体，一个指向writePos_，一个指向另一个buff
3.1 读取数据到这两个iovec中
3.1.1 如果buffer_剩余空间足够，直接写入，更新writePos_
3.1.2 如果buffer_剩余空间不够，把writePos指向buffer_末尾，把buff中的数据写到buffer_中（参考1.2）

写文件策略
直接写入就完事了，更新readPos_

*/


#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len); 
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);

    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
};

#endif //BUFFER_H