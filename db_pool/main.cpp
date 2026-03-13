#include "db_pool.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <format>

int main() {
    std::cout << "=== C++20 Database Connection Pool Demo ===\n";
    std::cout << "Features: Lifecycle management, Health check, Transaction\n\n";

    PoolConfig config;
    config.min_connections = 2;
    config.max_connections = 5;
    config.connection_timeout = std::chrono::seconds(10);
    config.idle_timeout = std::chrono::seconds(60);

    // 创建连接池
    auto pool = std::make_shared<ConnectionPool>(
        []() -> DBConnection::Ptr {
            return std::make_shared<MySQLConnection>();
        },
        config
    );

    pool->initialize();

    std::cout << "Pool initialized\n\n";

    // 模拟多线程使用连接池
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, i]() {
            try {
                auto conn = pool->borrow_connection();

                std::cout << "Thread " << i << " got connection\n";

                // 模拟数据库操作
                conn->execute("SELECT * FROM users WHERE id = " + std::to_string(i));

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                pool->return_connection(conn);

                std::cout << "Thread " << i << " returned connection\n";
            } catch (const std::exception& e) {
                std::cerr << "Thread " << i << " error: " << e.what() << "\n";
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 打印统计信息
    std::cout << "\n=== Pool Statistics ===\n";
    auto stats = pool->get_stats();
    std::cout << std::format("Active connections: {}\n", stats.active_connections);
    std::cout << std::format("Idle connections: {}\n", stats.idle_connections);
    std::cout << std::format("Total connections: {}\n", stats.total_connections);
    std::cout << std::format("Connections created: {}\n", stats.connections_created);
    std::cout << std::format("Connections borrowed: {}\n", stats.connections_borrowed);
    std::cout << std::format("Connections returned: {}\n", stats.connections_returned);

    pool->shutdown();

    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

