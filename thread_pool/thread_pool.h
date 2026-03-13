#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <compare>
#include <format>

// ============================================================================
// C++20 高性能线程池
// 特性：
// 1. 动态调整线程数量
// 2. 支持任务优先级
// 3. 支持 Future/Promise 模式
// 4. 优雅关闭机制
// ============================================================================

// 任务优先级
enum class TaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    URGENT = 3
};

// 任务包装器
struct Task {
    std::function<void()> function;
    TaskPriority priority;
    uint64_t sequence;  // 用于同优先级时的顺序

    // C++20 spaceship 运算符
    auto operator<=>(const Task& other) const {
        if (priority != other.priority)
            return other.priority <=> priority;  // 高优先级优先
        return other.sequence <=> sequence;  // 先提交的优先
    }
};

class ThreadPool {
public:
    explicit ThreadPool(size_t initial_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 提交任务（无返回值）
    template<typename F>
    void submit(F&& f) {
        submit_with_priority(std::forward<F>(f), TaskPriority::NORMAL);
    }

    // 提交任务（带优先级）
    template<typename F>
    void submit_with_priority(F&& f, TaskPriority priority) {
        using TaskType = std::packaged_task<void()>;
        auto task = std::shared_ptr<TaskType>(new TaskType(std::forward<F>(f)));

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // 检查是否需要增加线程
            if (pending_tasks_.size() >= thread_count_ * 2 &&
                thread_count_ < max_threads_) {
                add_worker_thread();
            }

            pending_tasks_.push(Task{
                .function = [task]() { (*task)(); },
                .priority = priority,
                .sequence = sequence_counter_++
            });
        }

        condition_.notify_one();
    }

    // 提交任务（有返回值）- Future 模式
    template<typename F>
    auto submit_future(F&& f) -> std::future<decltype(f())> {
        using ReturnType = decltype(f());
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            pending_tasks_.push(Task{
                .function = [task]() { (*task)(); },
                .priority = TaskPriority::NORMAL,
                .sequence = sequence_counter_++
            });
        }

        condition_.notify_one();
        return result;
    }

    // 动态调整线程数
    void set_thread_count(size_t new_count);
    size_t get_thread_count() const { return thread_count_.load(); }

    // 优雅关闭
    void shutdown();
    void wait_all();

    // 统计信息
    struct Stats {
        size_t total_tasks;
        size_t completed_tasks;
        size_t pending_tasks;
        size_t active_threads;
    };

    Stats get_stats() const {
        return Stats{
            .total_tasks = total_tasks_.load(),
            .completed_tasks = completed_tasks_.load(),
            .pending_tasks = pending_tasks_.size(),
            .active_threads = active_threads_.load()
        };
    }

private:
    void worker_loop();
    void add_worker_thread();
    void remove_worker_thread();

    std::vector<std::thread> workers_;
    std::priority_queue<Task> pending_tasks_;

    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;

    std::atomic<size_t> thread_count_{0};
    std::atomic<size_t> active_threads_{0};
    std::atomic<uint64_t> sequence_counter_{0};
    std::atomic<uint64_t> total_tasks_{0};
    std::atomic<uint64_t> completed_tasks_{0};

    std::atomic<bool> shutdown_{false};

    static constexpr size_t MIN_THREADS = 1;
    static constexpr size_t MAX_THREADS = 64;
    static constexpr size_t max_threads_ = MAX_THREADS;
};

