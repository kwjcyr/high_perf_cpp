#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <stacktrace>
#include <format>
#include <cstdint>

// ============================================================================
// C++20 内存泄漏检测器
// 特性：
// 1. 重载全局 new/delete
// 2. 追踪每一次内存分配
// 3. 程序结束时报告泄漏信息
// 4. 能定位到具体的文件和行号
// ============================================================================

struct AllocationInfo {
    size_t size;
    std::stacktrace trace;
    const char* file;
    int line;
    uint64_t timestamp;
};

class LeakDetector {
public:
    static LeakDetector& instance() {
        static LeakDetector detector;
        return detector;
    }

    void track_allocation(void* ptr, size_t size, const char* file, int line) {
        if (!enabled_.load(std::memory_order_relaxed)) return;

        std::lock_guard<std::mutex> lock(mutex_);
        allocations_[ptr] = AllocationInfo{
            .size = size,
            .trace = std::stacktrace::current(),
            .file = file,
            .line = line,
            .timestamp = total_allocations_.fetch_add(1, std::memory_order_relaxed)
        };
        current_memory_.fetch_add(size, std::memory_order_relaxed);
        peak_memory_.fetch_max(current_memory_.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
    }

    void track_deallocation(void* ptr) {
        if (!enabled_.load(std::memory_order_relaxed)) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            current_memory_.fetch_sub(it->second.size, std::memory_order_relaxed);
            allocations_.erase(it);
        }
    }

    void report_leaks() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (allocations_.empty()) {
            std::cout << "\n✓ No memory leaks detected!\n";
            return;
        }

        std::cout << std::format("\n{:=^60}\n", " MEMORY LEAK REPORT ");
        std::cout << std::format("Total allocations: {}\n", total_allocations_.load());
        std::cout << std::format("Leaked allocations: {}\n", allocations_.size());
        std::cout << std::format("Leaked memory: {} bytes\n", current_memory_.load());
        std::cout << std::format("Peak memory: {} bytes\n", peak_memory_.load());
        std::cout << std::format("{:=^60}\n\n", "");

        int count = 0;
        for (const auto& [ptr, info] : allocations_) {
            if (count++ >= 10) {
                std::cout << "... and more\n";
                break;
            }

            std::cout << std::format("Leak #{}:\n", count);
            std::cout << std::format("  Address: {:p}\n", ptr);
            std::cout << std::format("  Size: {} bytes\n", info.size);
            std::cout << std::format("  Location: {}:{}\n", info.file, info.line);
            std::cout << "  Stack trace:\n";

            for (const auto& frame : info.trace) {
                std::cout << "    " << frame.description() << "\n";
            }
            std::cout << "\n";
        }
    }

    void enable() { enabled_.store(true, std::memory_order_relaxed); }
    void disable() { enabled_.store(false, std::memory_order_relaxed); }

private:
    LeakDetector() : enabled_(true) {}
    ~LeakDetector() {
        if (enabled_.load()) {
            report_leaks();
        }
    }

    std::unordered_map<void*, AllocationInfo> allocations_;
    std::mutex mutex_;
    std::atomic<bool> enabled_;
    std::atomic<uint64_t> total_allocations_{0};
    std::atomic<size_t> current_memory_{0};
    std::atomic<size_t> peak_memory_{0};
};

// 重载全局 new/delete（需要链接时设置）
void* operator new(size_t size, const char* file, int line) {
    void* ptr = ::operator new(size);
    LeakDetector::instance().track_allocation(ptr, size, file, line);
    return ptr;
}

void* operator new[](size_t size, const char* file, int line) {
    void* ptr = ::operator new[](size);
    LeakDetector::instance().track_allocation(ptr, size, file, line);
    return ptr;
}

// 调试宏
#define DEBUG_NEW new(__FILE__, __LINE__)
#define new DEBUG_NEW

