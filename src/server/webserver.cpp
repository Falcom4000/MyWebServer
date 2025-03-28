#include <memory>
using namespace std;
#include "webserver.h"

WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd,
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize)
    : port_(port)
    , openLinger_(OptLinger)
    , timeoutMS_(timeoutMS)
    , isClose_(false)
    , timer_(new HeapTimer())
    , threadpool_(new ThreadPool())
    , epoller_(new Epoller())
    , iplist_(make_unique<iplist>("./iplist/ip.log"))
{
    const int PATH_MAX = 128; 
    char buff[PATH_MAX];
    if (getcwd(buff, PATH_MAX) != nullptr) {
        srcDir_ = std::string(buff) + "/resources";
    } else {
        // 处理错误情况
        srcDir_ = "./resources"; // 使用相对路径作为备选
        LOG_WARN("Failed to get current directory, using relative path");
    }
    LOG_INFO("srcDir: {}", srcDir_.c_str());
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    InitEventMode_(trigMode);
    if (!InitSocket_()) {
        isClose_ = true;
    }
    if (openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if (isClose_) {
            LOG_ERROR("========== Server init error!==========");
        } else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:{}, OpenLinger: {}", port_, OptLinger ? "true" : "false");
            LOG_INFO("Listen Mode: {}, OpenConn Mode: {}",
                (listenEvent_ & EPOLLET ? "ET" : "LT"),
                (connEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: {}", logLevel);
            LOG_INFO("srcDir: {}", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: {}, ThreadPool num: {}", connPoolNum, threadNum);
        }
    }
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    threadpool_->start();
}
WebServer::~WebServer()
{
    close(listenFd_);
    isClose_ = true;
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::SendError_(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[{}] error!", fd);
    }
    close(fd);
}

void WebServer::InitEventMode_(int trigMode)
{
    // 配置ET/LT模式
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode) {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start()
{
    int timeMS = -1; /* epoll wait timeout == -1 无事件将阻塞 */
    if (!isClose_) {
        LOG_INFO("========== Server start ==========");
    }
    while (!isClose_) {
        if (timeoutMS_ > 0) {
            timeMS = timer_->GetNextTimeout();
        }
        int eventCnt = epoller_->Wait(timeMS);
        for (int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if (fd == listenFd_) {
                DealListen_();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            } else if (events & EPOLLIN) {
                // 处理读事件
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            } else if (events & EPOLLOUT) {
                // 处理写事件
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::DealListen_()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);
        if (fd <= 0) {
            return;
        } else if (HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        iplist_->insert(addr);
        AddClient_(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void WebServer::AddClient_(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if (timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, make_shared<function<void()>>(bind(&WebServer::CloseConn_, this, &users_[fd])));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[{}] in!", users_[fd].GetFd());
}

void WebServer::CloseConn_(HttpConn* client)
{
    assert(client);
    LOG_INFO("Client[{}] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::DealRead_(HttpConn* client)
{
    assert(client);
    ExtentTime_(client);
    threadpool_->commit([this, client]() { WebServer::OnRead_(client); });
}

void WebServer::DealWrite_(HttpConn* client)
{
    assert(client);
    ExtentTime_(client);
    threadpool_->commit([this, client]() { WebServer::OnWrite_(client); });
}

void WebServer::ExtentTime_(HttpConn* client)
{
    assert(client);
    if (timeoutMS_ > 0) {
        timer_->reset(client->GetFd(), timeoutMS_);
    }
}

void WebServer::OnRead_(HttpConn* client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnWrite_(HttpConn* client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if (client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::OnProcess(HttpConn* client)
{
    if (client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

/* Create listenFd */
bool WebServer::InitSocket_()
{
    int ret;
    struct sockaddr_in addr;
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:{} error!", port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if (openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error! port={}", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error! port={}", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    int keep_alive = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("set socket keep_alive error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:{} error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG_ERROR("Listen port:{} error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:{}", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}
