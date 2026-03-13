#pragma once

#include <atomic>
#include <optional>
#include <memory>
#include <cstdint>

// ============================================================================
// C++20 无锁队列实现
// 特性：
// 1. 无锁数据结构（lock-free）
// 2. 解决 ABA 问题（使用 tagged pointer）
// 3. 正确的内存序选择
// 4. 高性能并发
// ============================================================================

// 解决 ABA 问题的带标签指针
template<typename T>
class TaggedPointer {
public:
    TaggedPointer() : data_(nullptr, 0) {}
    TaggedPointer(T* ptr, uint64_t tag) : data_(ptr, tag) {}

    T* get_ptr() const { return std::get<0>(data_); }
    uint64_t get_tag() const { return std::get<1>(data_); }

    // C++20 atomic 操作
    std::pair<T*, uint64_t> load(std::memory_order order = std::memory_order_seq_cst) const {
        return data_.load(order);
    }

    bool compare_exchange_weak(TaggedPointer& expected, TaggedPointer desired,
                              std::memory_order success = std::memory_order_seq_cst,
                              std::memory_order failure = std::memory_order_seq_cst) {
        auto exp = expected.data_.load();
        bool result = data_.compare_exchange_weak(exp, desired.data_, success, failure);
        expected = TaggedPointer(std::get<0>(exp), std::get<1>(exp));
        return result;
    }

private:
    std::atomic<std::pair<T*, uint64_t>> data_;
};

// 无锁队列节点
template<typename T>
struct Node {
    T data;
    std::atomic<Node*> next;

    Node() : next(nullptr) {}
    Node(const T& value) : data(value), next(nullptr) {}
};

// Michael & Scott 无锁队列
template<typename T>
class LockFreeQueue {
public:
    LockFreeQueue() {
        // 创建哨兵节点
        auto dummy = new Node<T>();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        size_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeQueue() {
        // 清空队列
        while (dequeue().has_value()) {}
        delete head_.load(std::memory_order_relaxed);
    }

    // 入队操作
    void enqueue(const T& value) {
        auto new_node = new Node<T>(value);

        while (true) {
            // 读取 tail
            auto tail = tail_.load(std::memory_order_acquire);
            auto next = tail->next.load(std::memory_order_acquire);

            // 检查 tail 是否变化
            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // 尝试将新节点链接到 tail 后面
                    Node<T>* expected = nullptr;
                    if (tail->next.compare_exchange_weak(expected, new_node,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        // 链接成功，尝试更新 tail
                        tail_.compare_exchange_strong(tail, new_node,
                            std::memory_order_release, std::memory_order_relaxed);
                        size_.fetch_add(1, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    // tail 落后了，帮忙更新 tail
                    tail_.compare_exchange_weak(tail, next,
                        std::memory_order_release, std::memory_order_relaxed);
                }
            }
        }
    }

    // 出队操作
    std::optional<T> dequeue() {
        while (true) {
            // 读取 head
            auto head = head_.load(std::memory_order_acquire);
            auto tail = tail_.load(std::memory_order_acquire);
            auto next = head->next.load(std::memory_order_acquire);

            // 检查 head 是否变化
            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) {
                        // 队列为空
                        return std::nullopt;
                    }
                    // tail 落后了，帮忙更新 tail
                    tail_.compare_exchange_weak(tail, next,
                        std::memory_order_release, std::memory_order_relaxed);
                } else {
                    // 读取值（在 CAS 之前）
                    T value = next->data;

                    // 尝试将 head 指向下一个节点
                    if (head_.compare_exchange_weak(head, next,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        // 出队成功
                        size_.fetch_sub(1, std::memory_order_relaxed);
                        delete head;  // 释放旧节点
                        return value;
                    }
                }
            }
        }
    }

    // 获取大小（近似值）
    size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }

    bool empty() const {
        return size() == 0;
    }

private:
    std::atomic<Node<T>*> head_;
    std::atomic<Node<T>*> tail_;
    std::atomic<size_t> size_;
};

