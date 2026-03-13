#include "db_pool.h"
#include <iostream>
#include <format>

ConnectionPool::ConnectionPool(ConnectionFactory factory, PoolConfig config)
    : factory_(std::move(factory)), config_(config) {}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

void ConnectionPool::initialize() {
    running_.store(true);

    // 创建最小连接数
    for (size_t i = 0; i < config_.min_connections; ++i) {
        auto conn = create_connection();
        if (conn) {
            idle_connections_.push(conn);
            all_connections_.push_back(conn);
        }
    }

    // 启动健康检查线程
    health_check_thread_ = std::thread(&ConnectionPool::health_check_thread, this);
}

void ConnectionPool::shutdown() {
    running_.store(false);
    pool_condition_.notify_all();

    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }

    // 销毁所有连接
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& conn : all_connections_) {
        destroy_connection(conn);
    }
    all_connections_.clear();

    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
}

DBConnection::Ptr ConnectionPool::borrow_connection() {
    std::unique_lock<std::mutex> lock(mutex_);

    auto deadline = std::chrono::steady_clock::now() + config_.connection_timeout;

    while (idle_connections_.empty()) {
        // 尝试创建新连接
        if (all_connections_.size() < config_.max_connections) {
            auto conn = create_connection();
            if (conn) {
                connections_borrowed_.fetch_add(1);
                return conn;
            }
        }

        // 等待连接归还
        if (pool_condition_.wait_until(lock, deadline) == std::cv_status::timeout) {
            throw std::runtime_error("Connection pool timeout");
        }
    }

    auto conn = idle_connections_.front();
    idle_connections_.pop();

    // 验证连接
    if (config_.validate_on_borrow && !validate_connection(conn)) {
        destroy_connection(conn);
        lock.unlock();
        return borrow_connection();  // 递归重试
    }

    connections_borrowed_.fetch_add(1);
    return conn;
}

void ConnectionPool::return_connection(DBConnection::Ptr conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (validate_connection(conn)) {
            idle_connections_.push(conn);
            pool_condition_.notify_one();
        } else {
            destroy_connection(conn);
        }
    }

    connections_returned_.fetch_add(1);
}

ConnectionPool::Stats ConnectionPool::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return Stats{
        .active_connections = all_connections_.size() - idle_connections_.size(),
        .idle_connections = idle_connections_.size(),
        .total_connections = all_connections_.size(),
        .connections_created = connections_created_.load(),
        .connections_destroyed = connections_destroyed_.load(),
        .connections_borrowed = connections_borrowed_.load(),
        .connections_returned = connections_returned_.load()
    };
}

void ConnectionPool::set_config(PoolConfig config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void ConnectionPool::health_check_thread() {
    while (running_.load()) {
        std::this_thread::sleep_for(config_.health_check_interval);

        std::unique_lock<std::mutex> lock(mutex_);

        for (auto it = all_connections_.begin(); it != all_connections_.end();) {
            auto conn = *it;

            if (!validate_connection(conn)) {
                destroy_connection(conn);
                it = all_connections_.erase(it);
            } else {
                ++it;
            }
        }

        // 缩容
        while (all_connections_.size() > config_.min_connections && !idle_connections_.empty()) {
            auto conn = idle_connections_.front();
            idle_connections_.pop();

            for (auto it = all_connections_.begin(); it != all_connections_.end(); ++it) {
                if (*it == conn) {
                    all_connections_.erase(it);
                    destroy_connection(conn);
                    break;
                }
            }
        }
    }
}

bool ConnectionPool::validate_connection(DBConnection::Ptr conn) {
    if (!conn || !conn->is_connected()) {
        return false;
    }
    return conn->ping();
}

DBConnection::Ptr ConnectionPool::create_connection() {
    try {
        auto conn = factory_();
        if (conn && conn->connect("")) {
            connections_created_.fetch_add(1);
            return conn;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create connection: " << e.what() << "\n";
    }
    return nullptr;
}

void ConnectionPool::destroy_connection(DBConnection::Ptr conn) {
    if (conn) {
        conn->disconnect();
        connections_destroyed_.fetch_add(1);
    }
}

void ConnectionPool::expand_pool() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (all_connections_.size() < config_.max_connections) {
        auto conn = create_connection();
        if (conn) {
            idle_connections_.push(conn);
            all_connections_.push_back(conn);
        }
    }
}

void ConnectionPool::shrink_pool() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (all_connections_.size() > config_.min_connections && !idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();

        auto it = std::find(all_connections_.begin(), all_connections_.end(), conn);
        if (it != all_connections_.end()) {
            all_connections_.erase(it);
            destroy_connection(conn);
        }
    }
}

// MySQLConnection 实现
bool MySQLConnection::connect(const std::string& conn_str) {
    conn_str_ = conn_str;
    connected_ = true;
    return true;
}

void MySQLConnection::disconnect() {
    connected_ = false;
}

bool MySQLConnection::is_connected() const {
    return connected_;
}

bool MySQLConnection::execute(const std::string& sql) {
    std::cout << "Executing SQL: " << sql << "\n";
    return true;
}

std::vector<std::unordered_map<std::string, std::string>> MySQLConnection::query(const std::string& sql) {
    std::cout << "Querying SQL: " << sql << "\n";
    return {};
}

bool MySQLConnection::ping() {
    return connected_;
}

void MySQLConnection::begin_transaction() {
    execute("BEGIN");
}

void MySQLConnection::commit() {
    execute("COMMIT");
}

void MySQLConnection::rollback() {
    execute("ROLLBACK");
}

