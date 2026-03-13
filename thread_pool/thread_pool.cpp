#include "thread_pool.h"
#include <iostream>
#include <chrono>
#include <format>
#include <ranges>

ThreadPool::ThreadPool(size_t initial_threads) {
    for (size_t i = 0; i < initial_threads; ++i) {
        add_worker_thread();
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // 等待任务或关闭信号
            condition_.wait(lock, [this]() {
                return shutdown_.load() || !pending_tasks_.empty();
            });

            if (shutdown_.load() && pending_tasks_.empty()) {
                return;
            }

            task = std::move(pending_tasks_.top());
            pending_tasks_.pop();
            total_tasks_.fetch_add(1, std::memory_order_relaxed);
        }

        active_threads_.fetch_add(1, std::memory_order_relaxed);

        try {
            task.function();
        } catch (const std::exception& e) {
            std::cerr << std::format("Task exception: {}\n", e.what());
        }

        active_threads_.fetch_sub(1, std::memory_order_relaxed);
        completed_tasks_.fetch_add(1, std::memory_order_relaxed);
    }
}

void ThreadPool::add_worker_thread() {
    if (thread_count_ >= max_threads_) return;

    workers_.emplace_back(&ThreadPool::worker_loop, this);
    thread_count_.fetch_add(1, std::memory_order_relaxed);
}

void ThreadPool::remove_worker_thread() {
    if (thread_count_ <= MIN_THREADS) return;

    condition_.notify_one();  // 唤醒一个线程退出
    thread_count_.fetch_sub(1, std::memory_order_relaxed);
}

void ThreadPool::set_thread_count(size_t new_count) {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    if (new_count > thread_count_.load()) {
        // 增加线程
        while (thread_count_ < new_count && thread_count_ < max_threads_) {
            add_worker_thread();
        }
    } else if (new_count < thread_count_.load()) {
        // 减少线程
        while (thread_count_ > new_count && thread_count_ > MIN_THREADS) {
            remove_worker_thread();
        }
        condition_.notify_all();
    }
}

void ThreadPool::shutdown() {
    shutdown_.store(true, std::memory_order_release);
    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPool::wait_all() {
    while (true) {
        auto stats = get_stats();
        if (stats.pending_tasks == 0 && stats.active_threads == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

