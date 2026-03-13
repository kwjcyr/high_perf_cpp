#include "double_buffer.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <format>

// 数据结构示例
struct FrameData {
    int frame_number;
    double timestamp;
    std::vector<float> vertices;

    FrameData() : frame_number(0), timestamp(0.0) {}
    FrameData(int num, double ts) : frame_number(num), timestamp(ts) {}
};

int main() {
    std::cout << "=== C++20 Double Buffer Demo ===\n";
    std::cout << "Features: Lock-free read, Copy-on-write, Thread-safe swap\n\n";

    // 测试 1：基本双缓冲
    std::cout << "=== Test 1: Basic Double Buffer ===\n";
    DoubleBuffer<int> buffer(0);

    std::cout << "Initial value: " << buffer.read() << "\n";

    // 写入新值
    buffer.write(100);
    std::cout << "After write: " << buffer.read() << "\n";

    // 使用函数修改
    buffer.modify([](int& value) {
        value += 50;
    });
    std::cout << "After modify: " << buffer.read() << "\n\n";

    // 测试 2：多线程读写
    std::cout << "=== Test 2: Multi-threaded Read/Write ===\n";
    DoubleBuffer<FrameData> frame_buffer;

    std::atomic<int> read_count{0};
    std::atomic<bool> running{true};

    // 写入线程（生产者）
    std::vector<std::thread> writers;
    for (int i = 0; i < 2; ++i) {
        writers.emplace_back([&frame_buffer, &running, i]() {
            for (int frame = 0; frame < 100 && running; ++frame) {
                FrameData data(frame, std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());

                data.vertices = {1.0f * i, 2.0f * i, 3.0f * i};
                frame_buffer.write(data);

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // 读取线程（消费者）
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&frame_buffer, &read_count, &running]() {
            while (running) {
                auto data = frame_buffer.readShared();
                if (data->frame_number > 0) {
                    read_count.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });
    }

    // 等待写入完成
    for (auto& w : writers) {
        w.join();
    }

    running = false;
    for (auto& r : readers) {
        r.join();
    }

    std::cout << "Total reads: " << read_count.load() << "\n";
    std::cout << "Final frame: " << frame_buffer.read().frame_number << "\n\n";

    // 测试 3：等待缓冲区交换
    std::cout << "=== Test 3: Wait for Buffer Swap ===\n";
    DoubleBuffer<int> swap_buffer(0);
    std::atomic<int> swap_count{0};

    std::thread waiter([&swap_buffer, &swap_count]() {
        for (int i = 0; i < 10; ++i) {
            if (swap_buffer.waitForSwap(std::chrono::milliseconds(100))) {
                std::cout << "Swap detected: " << swap_buffer.read() << "\n";
                swap_count.fetch_add(1);
            }
        }
    });

    for (int i = 1; i <= 10; ++i) {
        swap_buffer.write(i * 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    waiter.join();
    std::cout << "Total swaps: " << swap_count.load() << "\n\n";

    // 测试 4：环形缓冲区
    std::cout << "=== Test 4: Ring Buffer ===\n";
    RingBuffer<int, 16> ring_buffer;

    // 推入数据
    for (int i = 0; i < 20; ++i) {
        if (ring_buffer.push(i)) {
            std::cout << "Pushed: " << i << " (size: " << ring_buffer.size() << ")\n";
        } else {
            std::cout << "Buffer full, dropped: " << i << "\n";
        }
    }

    // 弹出数据
    std::cout << "\nPopping data:\n";
    while (!ring_buffer.empty()) {
        if (auto value = ring_buffer.pop()) {
            std::cout << "Popped: " << *value << " (size: " << ring_buffer.size() << ")\n";
        }
    }

    std::cout << "\nRing buffer stats:\n";
    std::cout << "  Capacity: " << RingBuffer<int, 16>::CAPACITY << "\n";
    std::cout << "  Current size: " << ring_buffer.size() << "\n";
    std::cout << "  Empty: " << (ring_buffer.empty() ? "yes" : "no") << "\n\n";

    // 测试 5：三缓冲
    std::cout << "=== Test 5: Triple Buffer ===\n";
    TripleBuffer<FrameData> triple_buffer;

    std::atomic<int> triple_write_count{0};
    std::atomic<int> triple_read_count{0};
    running = true;

    std::thread triple_writer([&triple_buffer, &triple_write_count, &running]() {
        for (int i = 0; i < 50 && running; ++i) {
            auto buf = triple_buffer.getWriteBuffer();
            buf->frame_number = i;
            buf->timestamp = i * 0.016;  // 60fps
            triple_buffer.commit();
            triple_write_count.fetch_add(1);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::thread triple_reader([&triple_buffer, &triple_read_count, &running]() {
        while (running) {
            const auto& data = triple_buffer.read();
            if (data.frame_number > 0) {
                triple_read_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    triple_writer.join();
    running = false;
    triple_reader.join();

    std::cout << "Triple buffer stats:\n";
    std::cout << "  Writes: " << triple_write_count.load() << "\n";
    std::cout << "  Reads: " << triple_read_count.load() << "\n";
    std::cout << "  Final frame: " << triple_buffer.read().frame_number << "\n\n";

    // 测试 6：性能对比
    std::cout << "=== Test 6: Performance Comparison ===\n";

    // 双缓冲性能
    DoubleBuffer<int> perf_double;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        perf_double.write(i);
        volatile auto val = perf_double.read();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto double_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Double Buffer: 1M ops in " << double_time << " ms\n";
    std::cout << "Throughput: " << (1000000 * 1000 / double_time) << " ops/sec\n";

    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

