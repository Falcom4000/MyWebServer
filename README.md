# MyWebServer

　　C++14 Webserver http://139.9.189.212:1316/

## 项目简介

实现了 Linux 上的轻量化 HTTP 服务器，使用单 Reactor 多线程并发模型，采用 Epoll 实现高效 IO 复用，配合缓冲区、定时器、日志系统、线程池与连接池，QPS 达到 10k+。

## 使用方法

1. 安装依赖库：

    ​`sudo apt-get install fmt mysql-server libmysqlclient-dev`​

2. 编译并运行：

    ```cpp
    mkdir build && cd build
    cmake ..
    make
    ./MyWebServer
    ```
3. 访问：

    通过浏览器或工具请求对应端口获取服务响应

## 架构设计

 * 单Reactor多线程模型，由一个Reactor负责监听连接请求和读写事件，分发给不同工作线程并行执行
 * Reactor采用Epoller进行IO复用，在处理完读事件后，会重新注册写事件
 * 读写事件都注册为LT，所以需要一直读写直到报错

## 主要模块

* 线程池 (ThreadPool)：统一管理和调度任务，避免频繁创建和销毁线程
* 缓冲区 (Buffer)：高效地组织数据的读写
* IO多路复用 (Epoller)：基于epoll的事件监听和管理
* 日志 (Log)：提供异步高效的日志记录能力
* SQL连接池：用于维护数据库连接，减少反复创建和销毁连接的损耗，提高系统性能。
* 定时器：用于定期处理超时任务或连接检测，保证服务器的稳定和高效运行。
* HTTP：管理HTTP连接，实现`request`​和`reponse`​

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
4. 如果buffer_剩余空间不够，把writePos指向buffer_末尾，把buff中的数据写到buffer_中

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

　　小根堆计时器，采用`unordered_map`​和`priority_queue`​管理定时器，采用延迟删除策略，最早到达的定时器优先。到达后优先执行超时任务。

## 总流程

### 服务器初始化

1. **配置加载**
   - 解析命令行参数，设置端口号、日志等级、触发模式等参数
   - 设置资源路径，确定静态文件目录位置

2. **资源准备**
   - 初始化日志系统，设置日志级别和写入方式
   - 创建并初始化数据库连接池，预先分配连接资源
   - 创建线程池，准备工作线程

3. **服务器设置**
   - 创建监听套接字(listenFd)，并设置为NONBLOCK
   - 设置套接字选项(SO_REUSEADDR等)
   - 绑定IP地址和端口号
   - 开始监听，设置连接队列大小

4. **事件管理器初始化**
   - 创建Epoller实例
   - 将监听套接字注册到Epoller中，监听连接请求事件
   - 初始化定时器，用于管理连接超时

5. **启动运行**
   - 开启事件循环，等待事件触发
   - 准备接受客户端连接请求

### HTTP请求响应流程

1. **连接建立**
   - 客户端发起连接请求，服务器主线程监听到连接事件
   - WebServer接收连接，为新连接创建HttpConn对象，分配文件描述符，并设置为NONBLOCK
   - 将新连接的fd注册到Epoller中，设置EPOLLIN事件监听
   - 添加定时器，用于处理连接超时

2. **请求接收与解析**
   - Epoller检测到读事件(EPOLLIN)，调用OnRead_函数
   - 由于设置了NONBLOCKING和LT，必须一直读到出现错误
   - 从socket读取数据到HttpConn的readBuff_缓冲区
   - 触发process()处理函数，解析HTTP请求:
     - HttpRequest::parse解析请求行、请求头、请求体
     - 识别请求方法(GET/POST)、路径、HTTP版本
     - 处理POST请求的表单数据(若有)

3. **响应生成**
   - 根据解析结果初始化HttpResponse，确定状态码(200/400等)
   - HttpResponse::MakeResponse生成HTTP响应:
     - 添加状态行(如"HTTP/1.1 200 OK")
     - 添加响应头(Content-type、Connection等)
     - 处理静态文件(使用sendfile实现零拷贝)
     - 设置响应内容

4. **响应发送**
   - 将fd重新注册为EPOLLOUT事件
   - Epoller检测到写事件，调用OnWrite_函数
   - 使用writev聚集写(iovec)，一次性写入响应头和文件内容
   - 根据写入结果更新缓冲区状态

5. **连接维护**
   - 检查Keep-Alive状态，决定是否保持连接
   - 若保持连接，重置定时器，将fd重新注册为EPOLLIN
   - 若关闭连接，调用CloseConn_函数清理资源
   - 定时器定期清理超时连接，避免资源泄露