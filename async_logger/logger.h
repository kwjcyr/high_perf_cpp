#pragma once

#include <string>
#include <string_view>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <chrono>
#include <format>
#include <memory>
#include <sstream>
#include <iomanip>
#include <ctime>

// ============================================================================
// C++20 高性能异步日志库
// 特性：
// 1. 异步写入，不阻塞业务线程
// 2. 支持日志轮转
// 3. 格式化输出性能优化
// 4. 崩溃时数据不丢失（双缓冲 + 同步刷新）
// ============================================================================

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id thread_id;
};

class AsyncLogger {
public:
    static AsyncLogger& instance() {
        static AsyncLogger logger;
        return logger;
    }

    void init(const std::string& filename, size_t buffer_size = 1024);
    void shutdown();

    // 日志记录接口
    template<typename... Args>
    void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        if (level < min_level_.load(std::memory_order_relaxed)) {
            return;
        }

        // 使用 C++20 std::format 进行格式化
        std::string message = std::format(fmt, std::forward<Args>(args)...);

        enqueue(LogEntry{
            .level = level,
            .message = std::move(message),
            .timestamp = std::chrono::system_clock::now(),
            .thread_id = std::this_thread::get_id()
        });
    }

    // 便捷方法
    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::DEBUG, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::INFO, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::WARNING, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::ERROR, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::FATAL, fmt, std::forward<Args>(args)...);
    }

    // 立即刷新日志
    void flush();

    // 设置最小日志级别
    void set_level(LogLevel level) {
        min_level_.store(level, std::memory_order_relaxed);
    }

    // 统计信息
    struct Stats {
        size_t total_logs;
        size_t dropped_logs;
        size_t file_rotations;
    };

    Stats get_stats() const { return stats_; }

private:
    AsyncLogger() = default;
    ~AsyncLogger();

    void enqueue(LogEntry entry);
    void background_thread();
    void write_to_file(const LogEntry& entry);
    void rotate_file();
    std::string level_to_string(LogLevel level);
    std::string format_timestamp(std::chrono::system_clock::time_point tp);
    std::string format_thread_id(std::thread::id tid);

    // 双缓冲设计
    std::queue<LogEntry> buffer1_, buffer2_;
    std::queue<LogEntry>* active_buffer_;
    std::queue<LogEntry>* inactive_buffer_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread background_thread_;
    std::atomic<bool> running_{false};
    std::atomic<LogLevel> min_level_{LogLevel::INFO};

    std::ofstream output_file_;
    std::string base_filename_;

    static constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB
    static constexpr size_t BUFFER_SIZE = 1024;

    Stats stats_{0, 0, 0};
};

// 日志宏
#define LOG_INFO(...) AsyncLogger::instance().info(__VA_ARGS__)
#define LOG_ERROR(...) AsyncLogger::instance().error(__VA_ARGS__)
#define LOG_WARNING(...) AsyncLogger::instance().warning(__VA_ARGS__)
#define LOG_DEBUG(...) AsyncLogger::instance().debug(__VA_ARGS__)
#define LOG_FATAL(...) AsyncLogger::instance().fatal(__VA_ARGS__)

