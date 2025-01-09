# MyWebServer

　　C++17 Webserver

    http://139.9.189.212:1316/

## 项目简介

　　该项目是一个基于C++17的轻量级Web服务器，目标是实现简洁高效的网络通信和请求处理，使用基于epoll的IO复用管理HTTP连接，定时器清除长时间的连接，缓冲区和阻塞队列辅助实现异步日志，SQL连接池管理SQL连接，线程池管理读写线程。


## 主要模块

* 线程池 (ThreadPool)：统一管理和调度任务，避免频繁创建和销毁线程
* 缓冲区 (Buffer)：高效地组织数据的读写
* IO多路复用 (Epoller)：基于epoll的事件监听和管理
* 日志 (Log)：提供异步高效的日志记录能力
* SQL连接池：用于维护数据库连接，减少反复创建和销毁连接的损耗，提高系统性能。
* 定时器：用于定期处理超时任务或连接检测，保证服务器的稳定和高效运行。
* HTTP：管理HTTP连接，实现`request`​和`reponse`​

## 使用方法

1. 安装依赖库：

    ​`sudo apt-get install fmt mysql-server libmysqlclient-dev`​

    注意：因为并非所有编译器都支持`std::format`​因此使用了第三方库`fmt`​
2. 编译并运行：

    ```cpp
    mkdir build && cd build
    cmake ..
    make
    ./MyWebServer
    ```
3. 访问：

    * 通过浏览器或工具请求对应端口获取服务响应

## 线程池的设计

1. 将所有任务存储到任务队列中，任务队列由阻塞队列实例化得到
2. 实例化若干个线程构成线程池，每个线程池上锁
3. 每个线程循环执行process函数
4. process函数：当队列非空，加锁，从队列中取出任务执行，返回

## Buffer的设计

1. 连续地址vector<char>存放数据
2. readPos_和writePos_标记已读和未读数据的边界，原子变量
3. 必须有readPos_ >= 0, writePos_ >= readPos_
   - 0 - readPos_ 之间是已读数据
   - readPos_ - writePos_ 之间是已写入未读数据
   - writePos_ - buffer.size() 之间是预留空间

### 写策略

1. 保证有足够的空间
   - 如果剩余空间不够，且预留空间 + 可写空间 < 待写入数据的长度，resize
   - 如果剩余空间不够，但预留空间 + 可写空间 >= 待写入数据的长度，移动数据
     - 具体来说，将已写入未读数据移动到buffer的起始位置, readPos_ = 0, writePos_ = readPos_ + 已写入未读数据长度
     - std::copy(first, last, result) 把[first, last)地址复制到result后

### 读策略

1. 全部读出到string中
   - len = writePos_ - readPos_
   - string str(Peek(), len) 从readPos_开始的len长度的数据复制到str中
   - 读出后，readPos_ = 0, writePos_ = 0
   - bzero(&buffer_[0], buffer_.size());

### 读文件策略

1. 分散读 定义两个iovec结构体，一个指向writePos_，一个指向另一个buff
2. 读取数据到这两个iovec中
3. 如果buffer_剩余空间足够，直接写入，更新writePos_
4. 如果buffer_剩余空间不够，把writePos指向buffer_末尾，把buff中的数据写到buffer_中（参考1.2）

### 写文件策略

　　直接写入就完事了，更新readPos_

## Epoller的设计

1. 提供对epoll的封装，实现注册fd，修改，删除，查询事件
2. 将事件暂存进vector中，对外提供查询接口
3. 避免内核事件表的fd和socket的fd混用
4. 避免直接暴露内核态接口

## 异步日志库的设计

1. 单例模式一个Log，里面控制着阻塞队列和Buffer
2. 用宏标记代码里的写入点
3. 创建日志文件：fopen一个日期.txt
4. 将时间 写入内容格式化成char*传给buffer里
5. buffer全部转为string，压入deque，用条件变量通知写线程
6. 写线程持续不断地尝试读string并写入log
7. 析构时先清空buffer，再清空deque，最后清空fp，并等待写线程返回。

## 定时器的设计

　　小根堆计时器，采用`unordered_map`​和`priority_queue`​管理定时器，采用延迟删除策略，最早到达的定时器优先。
