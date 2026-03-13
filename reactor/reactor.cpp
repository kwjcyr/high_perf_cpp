#include "reactor.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

Reactor::Reactor() {
#ifdef __APPLE__
    kq_ = -1;
#else
    epfd_ = -1;
#endif
}

Reactor::~Reactor() {
    stop();
#ifdef __APPLE__
    if (kq_ >= 0) close(kq_);
#else
    if (epfd_ >= 0) close(epfd_);
#endif
}

bool Reactor::init() {
#ifdef __APPLE__
    kq_ = kqueue();
    if (kq_ < 0) return false;
#else
    epfd_ = epoll_create1(0);
    if (epfd_ < 0) return false;
    events_.resize(MAX_EVENTS);
#endif

    // 创建唤醒管道
    if (pipe(wakeup_fd_) != 0) {
        return false;
    }

    fcntl(wakeup_fd_[0], F_SETFL, O_NONBLOCK);
    fcntl(wakeup_fd_[1], F_SETFL, O_NONBLOCK);

    return true;
}

void Reactor::run() {
    running_.store(true);

    while (running_.load()) {
#ifdef __APPLE__
        struct timespec timeout = {1, 0};
        std::vector<struct kevent> kevents(MAX_EVENTS);
        int n = kevent(kq_, nullptr, 0, kevents.data(), MAX_EVENTS, &timeout);

        for (int i = 0; i < n; i++) {
            auto& ev = kevents[i];
            if (ev.ident == (uintptr_t)wakeup_fd_[0]) {
                // 唤醒事件
                char dummy[8];
                read(wakeup_fd_[0], dummy, sizeof(dummy));

                // 执行任务
                std::vector<std::function<void()>> tasks;
                {
                    std::lock_guard<std::mutex> lock(tasks_mutex_);
                    while (!pending_tasks_.empty()) {
                        tasks.push_back(std::move(pending_tasks_.front()));
                        pending_tasks_.pop();
                    }
                }
                for (auto& task : tasks) {
                    task();
                }
            } else {
                // 其他事件
                auto it = connections_.find((int)ev.ident);
                if (it != connections_.end()) {
                    if (ev.filter == EVFILT_READ) {
                        it->second->on_read();
                    }
                    if (ev.filter == EVFILT_WRITE) {
                        it->second->on_write();
                    }
                }
            }
        }
#else
        int n = epoll_wait(epfd_, events_.data(), MAX_EVENTS, 1000);

        for (int i = 0; i < n; i++) {
            auto& ev = events_[i];
            if (ev.data.fd == wakeup_fd_[0]) {
                char dummy[8];
                read(wakeup_fd_[0], dummy, sizeof(dummy));

                std::vector<std::function<void()>> tasks;
                {
                    std::lock_guard<std::mutex> lock(tasks_mutex_);
                    while (!pending_tasks_.empty()) {
                        tasks.push_back(std::move(pending_tasks_.front()));
                        pending_tasks_.pop();
                    }
                }
                for (auto& task : tasks) {
                    task();
                }
            } else {
                auto it = connections_.find(ev.data.fd);
                if (it != connections_.end()) {
                    if (ev.events & EPOLLIN) {
                        it->second->on_read();
                    }
                    if (ev.events & EPOLLOUT) {
                        it->second->on_write();
                    }
                }
            }
        }
#endif
    }
}

void Reactor::stop() {
    running_.store(false);
    // 唤醒 Reactor 线程
    char dummy = 1;
    write(wakeup_fd_[1], &dummy, 1);
}

void Reactor::post(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        pending_tasks_.push(std::move(task));
    }
    char dummy = 1;
    write(wakeup_fd_[1], &dummy, 1);
}

int Reactor::create_server_socket(const std::string& host, int port) {
#ifdef __APPLE__
    int fd = socket(AF_INET, SOCK_STREAM, 0);
#else
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
#endif
    if (fd < 0) throw std::system_error(errno, std::generic_category());

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        throw std::system_error(errno, std::generic_category());
    }

    if (listen(fd, SOMAXCONN) < 0) {
        close(fd);
        throw std::system_error(errno, std::generic_category());
    }

#ifdef __APPLE__
    // macOS 需要单独设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

    return fd;
}

void Reactor::add_connection(int fd, Connection::Ptr conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_[fd] = std::move(conn);

    // 注册读事件
#ifdef __APPLE__
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    kevent(kq_, &ev, 1, nullptr, 0, nullptr);
#endif
}

void Reactor::remove_connection(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(fd);
}

Connection::Ptr Reactor::get_connection(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    return it != connections_.end() ? it->second : nullptr;
}

void Reactor::register_event(int fd, EventType events, std::function<void()> callback) {
    post([this, fd, events, callback]() {
        callbacks_[fd] = callback;

#ifdef __APPLE__
        struct kevent ev;
        short filter = 0;
        if (events == EventType::READ || static_cast<int>(events) & static_cast<int>(EventType::READ)) {
            filter = EVFILT_READ;
        }
        EV_SET(&ev, fd, filter, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        kevent(kq_, &ev, 1, nullptr, 0, nullptr);
#endif
    });
}

void Reactor::unregister_event(int fd) {
    post([this, fd]() {
        callbacks_.erase(fd);

#ifdef __APPLE__
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        kevent(kq_, &ev, 1, nullptr, 0, nullptr);
#endif
    });
}

void Reactor::modify_event(int fd, EventType events) {
    post([this, fd, events]() {
#ifdef __APPLE__
        struct kevent ev;
        short filter = (static_cast<int>(events) & static_cast<int>(EventType::WRITE)) ? EVFILT_WRITE : EVFILT_READ;
        EV_SET(&ev, fd, filter, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        kevent(kq_, &ev, 1, nullptr, 0, nullptr);
#endif
    });
}

TCPServer::TCPServer(Reactor& reactor) : reactor_(reactor) {}

bool TCPServer::bind(const std::string& host, int port) {
    server_fd_ = reactor_.create_server_socket(host, port);
    return server_fd_ >= 0;
}

void TCPServer::start() {
    if (server_fd_ < 0) return;

    reactor_.register_event(server_fd_, EventType::READ, [this]() {
        struct sockaddr_in client_addr = {};
        socklen_t client_len = sizeof(client_addr);

        while (true) {
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) break;

            fcntl(client_fd, F_SETFL, O_NONBLOCK);

            if (on_connection_) {
                auto conn = std::make_shared<TCPConnection>(client_fd, reactor_);
                reactor_.add_connection(client_fd, conn);
                on_connection_(conn);
            }
        }
    });
}

void TCPServer::stop() {
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
}

void TCPServer::on_connection(ConnectionHandler handler) {
    on_connection_ = std::move(handler);
}

TCPConnection::TCPConnection(int fd, Reactor& reactor)
    : fd_(fd), reactor_(reactor) {}

void TCPConnection::on_read() {
    char buffer[4096];
    ssize_t n = ::recv(fd_, buffer, sizeof(buffer), 0);
    if (n > 0) {
        read_buffer_.append(buffer, n);
        std::cout << "Received: " << read_buffer_ << "\n";
    } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
        on_close();
    }
}

void TCPConnection::on_write() {
    if (write_buffer_.empty()) return;

    ssize_t n = ::send(fd_, write_buffer_.data(), write_buffer_.size(), 0);
    if (n > 0) {
        write_buffer_.erase(0, n);
    }
}

void TCPConnection::on_close() {
    reactor_.remove_connection(fd_);
    close(fd_);
}

void TCPConnection::send(const std::string& data) {
    write_buffer_ += data;
    reactor_.modify_event(fd_, EventType::WRITE);
}

std::string TCPConnection::recv(size_t max_size) {
    std::string result = read_buffer_.substr(0, max_size);
    read_buffer_.erase(0, max_size);
    return result;
}

