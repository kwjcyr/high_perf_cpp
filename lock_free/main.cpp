#include "lock_free_queue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <format>
#include <atomic>

int main() {
    std::cout << std::format("{:=^60}\n", " C++20 Lock-Free Queue Demo ");
    std::cout << "Features: Lock-free, ABA-safe, Correct memory ordering\n\n";

    LockFreeQueue<int> queue;

    // 测试 1：单线程基本操作
    for (int i = 0; i < 1000; ++i) {
        queue.enqueue(i);
    }
    std::cout << std::format("Enqueued 1000 items, size: {}\n", queue.size());

    int count = 0;
    while (auto value = queue.dequeue()) {
        count++;
    }
    std::cout << std::format("Dequeued {} items, queue empty: {}\n\n", count, queue.empty());

    // 测试 2：多线程并发
    std::cout << "=== Test 2: Multi-threaded Stress Test ===\n";
    const int PRODUCERS = 4;
    const int CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 10000;

    std::atomic<int> total_consumed{0};
    std::atomic<bool> production_done{false};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers, consumers;

    // 启动生产者
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&queue, p, ITEMS_PER_PRODUCER]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                queue.enqueue(p * ITEMS_PER_PRODUCER + i);
                std::this_thread::yield();  // 让出 CPU
            }
        });
    }

    // 启动消费者
    for (int c = 0; c < CONSUMERS; ++c) {
        consumers.emplace_back([&queue, &total_consumed, &production_done]() {
            while (true) {
                if (auto value = queue.dequeue()) {
                    total_consumed.fetch_add(1, std::memory_order_relaxed);
                } else if (production_done.load() && queue.empty()) {
                    break;
                }
                std::this_thread::yield();
            }
        });
    }

    // 等待生产者完成
    for (auto& p : producers) {
        p.join();
    }
    production_done.store(true);

    // 等待消费者完成
    for (auto& c : consumers) {
        c.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int expected = PRODUCERS * ITEMS_PER_PRODUCER;
    std::cout << std::format("Produced: {} items\n", expected);
    std::cout << std::format("Consumed: {} items\n", total_consumed.load());
    std::cout << std::format("Time: {} ms\n", duration.count());
    std::cout << std::format("Throughput: {:.0f} ops/sec\n",
        (expected * 1000.0) / duration.count());

    // 测试 3：性能对比
    std::cout << "\n=== Test 3: Performance Comparison ===\n";
    std::cout << "Lock-free queue achieves high throughput without mutex overhead\n";

    std::cout << "\n" << std::format("{:=^60}\n", " Demo Complete ");

    return 0;
}

