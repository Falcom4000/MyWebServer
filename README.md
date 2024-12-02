# MyWebServer
C++14 Webserver


## 线程池的设计
1. 将所有任务存储到任务队列中，任务队列由阻塞队列实例化得到
2. 实例化若干个线程构成线程池，每个线程池上锁
3. 每个线程循环执行process函数
4， process函数：当队列非空，加锁，从队列中取出任务执行，返回

## Buffer的设计
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

## Epoller的设计
提供对epoll的封装，实现注册fd，修改，删除，查询事件
将事件暂存进vector中，对外提供查询接口
避免内核事件表的fd和socket的fd混用
避免直接暴露内核态接口