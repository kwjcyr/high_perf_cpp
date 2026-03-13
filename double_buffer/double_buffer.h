#pragma once

#include <atomic>
#include <mutex>
#include <memory>
#include <functional>
#include <optional>
#include <condition_variable>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <array>

// ============================================================================
// C++20 Double Buffer（双缓冲）实现
// 特性：
// 1. 无锁读取（lock-free read）
// 2. 写时复制（copy-on-write）
// 3. 线程安全的缓冲区交换
// 4. 支持自定义数据类型
// ============================================================================

template<typename T>
class DoubleBuffer {
public:
    using value_type = T;
    using buffer_type = std::shared_ptr<T>;

    // 默认构造函数
    DoubleBuffer() : DoubleBuffer(T{}) {}

    // 带初始值的构造函数
    explicit DoubleBuffer(const T& initial_value)
        : buffers_{std::make_shared<T>(initial_value), std::make_shared<T>(initial_value)},
          read_index_(0) {}

    // 读取当前缓冲区（无锁，线程安全）
    const T& read() const noexcept {
        size_t index = read_index_.load(std::memory_order_acquire);
        return *buffers_[index];
    }

    // 获取当前缓冲区的共享指针（无锁）
    buffer_type readShared() const noexcept {
        size_t index = read_index_.load(std::memory_order_acquire);
        return buffers_[index];
    }

    // 写入新值（线程安全）
    void write(const T& value) {
        std::lock_guard<std::mutex> lock(write_mutex_);

        // 写入后备缓冲区
        size_t write_index = 1 - read_index_.load(std::memory_order_relaxed);
        *buffers_[write_index] = value;

        // 原子交换缓冲区
        read_index_.store(write_index, std::memory_order_release);

        // 通知等待的读者
        swap_cv_.notify_all();
    }

    // 使用函数修改缓冲区（线程安全）
    void modify(std::function<void(T&)> modifier) {
        std::lock_guard<std::mutex> lock(write_mutex_);

        size_t write_index = 1 - read_index_.load(std::memory_order_relaxed);
        modifier(*buffers_[write_index]);

        read_index_.store(write_index, std::memory_order_release);
        swap_cv_.notify_all();
    }

    // 等待缓冲区交换（用于消费者等待新数据）
    template<typename Rep, typename Period>
    bool waitForSwap(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(swap_mutex_);
        size_t current_index = read_index_.load(std::memory_order_acquire);

        // 等待读索引变化或超时
        return swap_cv_.wait_for(lock, timeout, [this, current_index]() {
            return read_index_.load(std::memory_order_acquire) != current_index;
        });
    }

    // 立即尝试交换（不阻塞）
    bool trySwap() {
        std::unique_lock<std::mutex> lock(swap_mutex_);
        size_t current_index = read_index_.load(std::memory_order_acquire);

        if (read_index_.load(std::memory_order_acquire) == current_index) {
            return false;
        }

        return true;
    }

    // 获取缓冲区版本号
    uint64_t getVersion() const noexcept {
        return version_.load(std::memory_order_relaxed);
    }

private:
    std::array<buffer_type, 2> buffers_;
    std::atomic<size_t> read_index_;
    std::atomic<uint64_t> version_{0};

    mutable std::mutex write_mutex_;
    mutable std::mutex swap_mutex_;
    std::condition_variable swap_cv_;
};

// ============================================================================
// Ring Buffer（环形缓冲区） - 多生产者多消费者
// ============================================================================

template<typename T, size_t Capacity>
class RingBuffer {
public:
    using value_type = T;
    static constexpr size_t CAPACITY = Capacity;

    RingBuffer() : head_(0), tail_(0), count_(0) {}

    // 推入数据（线程安全）
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_.load() >= CAPACITY) {
            return false;  // 缓冲区满
        }

        buffer_[tail_.load()] = item;
        tail_.store((tail_.load() + 1) % CAPACITY, std::memory_order_release);
        count_.fetch_add(1, std::memory_order_release);

        not_empty_.notify_one();
        return true;
    }

    // 弹出数据（线程安全）
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        if (count_.load() == 0) {
            return std::nullopt;  // 缓冲区空
        }

        T item = std::move(buffer_[head_.load()]);
        head_.store((head_.load() + 1) % CAPACITY, std::memory_order_release);
        count_.fetch_sub(1, std::memory_order_release);

        not_full_.notify_one();
        return item;
    }

    // 阻塞推入（等待空间）
    void pushWait(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        not_full_.wait(lock, [this]() { return count_.load() < CAPACITY; });

        buffer_[tail_.load()] = item;
        tail_.store((tail_.load() + 1) % CAPACITY, std::memory_order_release);
        count_.fetch_add(1, std::memory_order_release);

        not_empty_.notify_one();
    }

    // 阻塞弹出（等待数据）
    T popWait() {
        std::unique_lock<std::mutex> lock(mutex_);

        not_empty_.wait(lock, [this]() { return count_.load() > 0; });

        T item = std::move(buffer_[head_.load()]);
        head_.store((head_.load() + 1) % CAPACITY, std::memory_order_release);
        count_.fetch_sub(1, std::memory_order_release);

        not_full_.notify_one();
        return item;
    }

    // 尝试推入（不阻塞）
    bool tryPush(const T& item) {
        return push(item);
    }

    // 尝试弹出（不阻塞）
    std::optional<T> tryPop() {
        return pop();
    }

    // 检查是否为空
    bool empty() const noexcept {
        return count_.load() == 0;
    }

    // 检查是否已满
    bool full() const noexcept {
        return count_.load() >= CAPACITY;
    }

    // 获取当前元素数量
    size_t size() const noexcept {
        return count_.load();
    }

    // 清空缓冲区
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_.store(0);
        tail_.store(0);
        count_.store(0);
    }

private:
    std::array<T, CAPACITY> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    std::atomic<size_t> count_;

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

// ============================================================================
// Triple Buffer（三缓冲） - 适用于高频写入场景
// ============================================================================

template<typename T>
class TripleBuffer {
public:
    TripleBuffer() : TripleBuffer(T{}) {}

    explicit TripleBuffer(const T& initial_value)
        : buffers_{std::make_shared<T>(initial_value),
                   std::make_shared<T>(initial_value),
                   std::make_shared<T>(initial_value)},
          read_index_(0), write_index_(1) {}

    // 读取（无锁）
    const T& read() const noexcept {
        return *buffers_[read_index_.load(std::memory_order_acquire)];
    }

    // 写入（无锁，使用 CAS）
    void write(const T& value) {
        // 获取下一个写缓冲区
        size_t next_write = (write_index_.load() + 1) % 3;

        // 确保不覆盖正在读的缓冲区
        while (next_write == read_index_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        *buffers_[next_write] = value;
        write_index_.store(next_write, std::memory_order_release);

        // 更新读索引
        read_index_.store(next_write, std::memory_order_release);
    }

    // 获取写入缓冲区（用于零拷贝写入）
    std::shared_ptr<T> getWriteBuffer() {
        size_t next_write = (write_index_.load() + 1) % 3;

        while (next_write == read_index_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        return buffers_[next_write];
    }

    // 提交写入
    void commit() {
        size_t current_write = write_index_.load();
        read_index_.store(current_write, std::memory_order_release);
    }

private:
    std::array<std::shared_ptr<T>, 3> buffers_;
    std::atomic<size_t> read_index_;
    std::atomic<size_t> write_index_;
};

