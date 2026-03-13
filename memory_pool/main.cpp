#include "memory_pool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <format>

struct TestObject {
    int64_t data[10];
    TestObject() {
        for (auto& v : data) v = 42;
    }
};

int main() {
    std::cout << "=== C++20 Memory Pool Demo ===\n\n";

    auto& pool = MemoryPool::instance();

    std::cout << "=== Test 1: Single Thread Performance ===\n";
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<void*> ptrs;
    for (int i = 0; i < 10000; ++i) {
        ptrs.push_back(pool.allocate(64));
    }
    for (auto ptr : ptrs) {
        pool.deallocate(ptr, 64);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Allocated and freed 10000 blocks in " << duration.count() << " ms\n";

    std::cout << "\n=== Test 2: Multi-threaded Performance ===\n";
    const int THREAD_COUNT = 4;
    const int OPS_PER_THREAD = 10000;

    std::vector<std::thread> threads;
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&, i]() {
            std::vector<void*> local_ptrs;
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                void* ptr = pool.allocate(64 + (i * 8));
                local_ptrs.push_back(ptr);
            }
            for (auto ptr : local_ptrs) {
                pool.deallocate(ptr, 64);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << THREAD_COUNT << " threads, " << OPS_PER_THREAD
              << " ops each, total time: " << duration.count() << " ms\n";

    std::cout << "\n=== Test 3: POOL_NEW Macro ===\n";
    TestObject* obj = POOL_NEW(TestObject);
    std::cout << "Created object at: " << obj << "\n";
    POOL_DELETE(obj);
    std::cout << "Object deleted\n";

    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

