#include "thread_pool.h"
#include <iostream>
#include <chrono>
#include <format>
#include <vector>
#include <numeric>

int main() {
    std::cout << std::format("{:=^60}\n", " C++20 ThreadPool Demo ");
    std::cout << "Features: Dynamic sizing, Priority, Future/Promise\n\n";

    ThreadPool pool(4);  // 初始 4 个线程

    // 测试 1：基本任务提交
    std::cout << "=== Test 1: Basic Task Submission ===\n";
    for (int i = 0; i < 10; ++i) {
        pool.submit([i]() {
            std::cout << "Task " << i << " executed by thread " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    pool.wait_all();

    // 测试 2：Future 模式
    std::cout << "\n=== Test 2: Future/Promise Pattern ===\n";
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 5; ++i) {
        auto future = pool.submit_future([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return i * i;
        });
        futures.push_back(std::move(future));
    }

    std::cout << "Results: ";
    for (auto& f : futures) {
        std::cout << f.get() << " ";
    }
    std::cout << "\n";

    // 测试 3：任务优先级
    std::cout << "\n=== Test 3: Task Priority ===\n";
    pool.submit_with_priority(
        []() {
            std::cout << "URGENT task executed\n";
        },
        TaskPriority::URGENT
    );
    pool.submit_with_priority(
        []() {
            std::cout << "LOW priority task executed\n";
        },
        TaskPriority::LOW
    );
    pool.submit(
        []() {
            std::cout << "NORMAL task executed\n";
        }
    );
    pool.wait_all();

    // 测试 4：动态调整线程数
    std::cout << "\n=== Test 4: Dynamic Thread Adjustment ===\n";
    auto stats = pool.get_stats();
    std::cout << "Initial threads: " << stats.active_threads << "\n";

    pool.set_thread_count(8);
    std::cout << "After scaling: " << pool.get_thread_count() << " threads\n";

    // 提交更多任务
    for (int i = 0; i < 20; ++i) {
        pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });
    }
    pool.wait_all();

    pool.set_thread_count(2);
    std::cout << "After scaling down: " << pool.get_thread_count() << " threads\n";

    // 测试 5：统计信息
    std::cout << "\n=== Final Statistics ===\n";
    stats = pool.get_stats();
    std::cout << "Total tasks: " << stats.total_tasks << "\n";
    std::cout << "Completed tasks: " << stats.completed_tasks << "\n";
    std::cout << "Current threads: " << pool.get_thread_count() << "\n";

    pool.shutdown();
    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

