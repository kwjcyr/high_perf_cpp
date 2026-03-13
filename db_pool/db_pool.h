#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <thread>

// ============================================================================
// C++20 数据库连接池
// 特性：
// 1. 连接的生命周期管理
// 2. 连接健康检查
// 3. 事务处理
// 4. 连接复用策略
// ============================================================================

// 连接状态
enum class ConnectionState {
    IDLE,
    ACTIVE,
    INVALID
};

// 数据库连接接口
class DBConnection {
public:
    using Ptr = std::shared_ptr<DBConnection>;

    virtual ~DBConnection() = default;
    virtual bool connect(const std::string& conn_str) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual bool execute(const std::string& sql) = 0;
    virtual std::vector<std::unordered_map<std::string, std::string>> query(const std::string& sql) = 0;
    virtual bool ping() = 0;  // 健康检查
    virtual void begin_transaction() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;
};

// 连接池配置
struct PoolConfig {
    size_t min_connections = 5;
    size_t max_connections = 20;
    std::chrono::seconds connection_timeout = std::chrono::seconds(30);
    std::chrono::seconds idle_timeout = std::chrono::seconds(300);
    std::chrono::seconds health_check_interval = std::chrono::seconds(60);
    bool validate_on_borrow = true;
};

// 连接池
class ConnectionPool {
public:
    using ConnectionFactory = std::function<DBConnection::Ptr()>;

    explicit ConnectionPool(ConnectionFactory factory, PoolConfig config = {});
    ~ConnectionPool();

    // 获取连接
    DBConnection::Ptr borrow_connection();

    // 归还连接
    void return_connection(DBConnection::Ptr conn);

    // 统计信息
    struct Stats {
        size_t active_connections;
        size_t idle_connections;
        size_t total_connections;
        size_t connections_created;
        size_t connections_destroyed;
        size_t connections_borrowed;
        size_t connections_returned;
    };

    Stats get_stats() const;

    // 配置管理
    void set_config(PoolConfig config);
    PoolConfig get_config() const { return config_; }

    // 连接池管理
    void initialize();
    void shutdown();
    void expand_pool();
    void shrink_pool();

private:
    void health_check_thread();
    bool validate_connection(DBConnection::Ptr conn);
    DBConnection::Ptr create_connection();
    void destroy_connection(DBConnection::Ptr conn);

    ConnectionFactory factory_;
    PoolConfig config_;

    std::vector<DBConnection::Ptr> all_connections_;
    std::queue<DBConnection::Ptr> idle_connections_;

    mutable std::mutex mutex_;
    std::condition_variable pool_condition_;

    std::atomic<bool> running_{false};
    std::thread health_check_thread_;

    // 统计
    std::atomic<size_t> connections_created_{0};
    std::atomic<size_t> connections_destroyed_{0};
    std::atomic<size_t> connections_borrowed_{0};
    std::atomic<size_t> connections_returned_{0};
};

// 简单的 MySQL 连接实现示例
class MySQLConnection : public DBConnection {
public:
    bool connect(const std::string& conn_str) override;
    void disconnect() override;
    bool is_connected() const override;
    bool execute(const std::string& sql) override;
    std::vector<std::unordered_map<std::string, std::string>> query(const std::string& sql) override;
    bool ping() override;
    void begin_transaction() override;
    void commit() override;
    void rollback() override;

private:
    bool connected_ = false;
    std::string conn_str_;
};

