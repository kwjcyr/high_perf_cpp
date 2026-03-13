#include "logger.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <format>

int main() {
    auto& logger = AsyncLogger::instance();
    logger.init("app.log");
    logger.set_level(LogLevel::DEBUG);

    std::cout << std::format("{:=^60}\n", " C++20 Async Logger Demo ");
    std::cout << "Features: Async write, Log rotation, Double buffering\n\n";

    // 测试 1：基本日志记录
    std::cout << "=== Test 1: Basic Logging ===\n";
    LOG_DEBUG("Debug message at {}", 2024);
    LOG_INFO("Application started");
    LOG_WARNING("This is a warning");
    LOG_ERROR("Error code: {}", 404);

    // 测试 2：多线程日志
    std::cout << "\n=== Test 2: Multi-threaded Logging ===\n";
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 1000; ++j) {
                LOG_INFO("Thread {} - Log message #{}", i, j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = logger.get_stats();
    std::cout << std::format("Total logs: {}\n", stats.total_logs);
    std::cout << std::format("Dropped logs: {}\n", stats.dropped_logs);

    // 测试 3：性能测试
    std::cout << "\n=== Test 3: Performance Test ===\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        LOG_INFO("Performance test #{}", i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 等待异步写入

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << std::format("Logged 10000 messages in {} ms\n", duration.count());
    std::cout << std::format("Throughput: {:.0f} logs/sec\n",
        (10000 * 1000.0) / duration.count());

    // 测试 4：不同日志级别
    std::cout << "\n=== Test 4: Log Levels ===\n";
    LOG_DEBUG("This is DEBUG level");
    LOG_INFO("This is INFO level");
    LOG_WARNING("This is WARNING level");
    LOG_ERROR("This is ERROR level");
    LOG_FATAL("This is FATAL level");

    logger.shutdown();
    std::cout << "\n" << std::format("{:=^60}\n", " Demo Complete ");
    std::cout << "Check 'app.log' file for detailed logs\n";

    return 0;
}

