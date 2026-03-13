#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <system_error>
#include <cstring>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// ============================================================================
// C++20 Reactor 模式网络库
// 特性：
// 1. 支持 epoll/kqueue（跨平台）
// 2. 高并发连接处理
// 3. 事件驱动架构
// 4. 连接池管理
// ============================================================================

// 事件类型
enum class EventType {
    READ = 1,
    WRITE = 2,
    ERROR = 4,
    ACCEPT = 8
};

// 连接接口
class Connection {
public:
    using Ptr = std::shared_ptr<Connection>;

    virtual ~Connection() = default;
    virtual void on_read() = 0;
    virtual void on_write() = 0;
    virtual void on_close() = 0;
    virtual int get_fd() const = 0;
};

// Reactor 类
class Reactor {
public:
    Reactor();
    ~Reactor();

    bool init();
    void run();
    void stop();

    // 注册事件
    void register_event(int fd, EventType events, std::function<void()> callback);
    void unregister_event(int fd);
    void modify_event(int fd, EventType events);

    // 添加任务（在 Reactor 线程执行）
    void post(std::function<void()> task);

    // 创建服务器套接字
    int create_server_socket(const std::string& host, int port);

    // 连接管理
    void add_connection(int fd, Connection::Ptr conn);
    void remove_connection(int fd);
    Connection::Ptr get_connection(int fd);

private:
#ifdef __APPLE__
    int kq_;  // kqueue fd
    std::vector<struct kevent> events_;
#else
    int epfd_;  // epoll fd
    std::vector<struct epoll_event> events_;
#endif

    std::atomic<bool> running_{false};
    int wakeup_fd_[2];

    std::mutex connections_mutex_;
    std::unordered_map<int, Connection::Ptr> connections_;

    std::mutex tasks_mutex_;
    std::queue<std::function<void()>> pending_tasks_;

    std::unordered_map<int, std::function<void()>> callbacks_;

    static constexpr int MAX_EVENTS = 1024;
};

// TCP 服务器
class TCPServer : public std::enable_shared_from_this<TCPServer> {
public:
    using ConnectionHandler = std::function<void(Connection::Ptr)>;

    TCPServer(Reactor& reactor);

    bool bind(const std::string& host, int port);
    void start();
    void stop();

    void on_connection(ConnectionHandler handler);

private:
    Reactor& reactor_;
    int server_fd_ = -1;
    ConnectionHandler on_connection_;
};

// 简单的 Connection 实现
class TCPConnection : public Connection {
public:
    TCPConnection(int fd, Reactor& reactor);

    void on_read() override;
    void on_write() override;
    void on_close() override;
    int get_fd() const override { return fd_; }

    void send(const std::string& data);
    std::string recv(size_t max_size = 1024);

private:
    int fd_;
    Reactor& reactor_;
    std::string read_buffer_;
    std::string write_buffer_;
};

