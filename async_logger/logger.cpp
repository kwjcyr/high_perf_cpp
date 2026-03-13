#include "logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

void AsyncLogger::init(const std::string& filename, size_t buffer_size) {
    base_filename_ = filename;
    output_file_.open(filename, std::ios::app);

    if (!output_file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }

    active_buffer_ = &buffer1_;
    inactive_buffer_ = &buffer2_;

    running_.store(true, std::memory_order_release);
    background_thread_ = std::thread(&AsyncLogger::background_thread, this);
}

void AsyncLogger::shutdown() {
    running_.store(false, std::memory_order_release);
    cv_.notify_one();

    if (background_thread_.joinable()) {
        background_thread_.join();
    }

    flush();
    output_file_.close();
}

void AsyncLogger::enqueue(LogEntry entry) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (active_buffer_->size() >= BUFFER_SIZE) {
        stats_.dropped_logs++;
        return;
    }

    active_buffer_->push(std::move(entry));
    stats_.total_logs++;

    cv_.notify_one();
}

void AsyncLogger::background_thread() {
    while (running_.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待有数据或超时
        cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return !active_buffer_->empty() || !running_.load();
        });

        // 交换缓冲区
        std::swap(active_buffer_, inactive_buffer_);
        lock.unlock();

        // 写入文件
        while (!inactive_buffer_->empty()) {
            write_to_file(inactive_buffer_->front());
            inactive_buffer_->pop();
        }
    }

    // 刷新剩余数据
    flush();
}

void AsyncLogger::write_to_file(const LogEntry& entry) {
    // 检查是否需要轮转
    if (output_file_.tellp() > MAX_FILE_SIZE) {
        rotate_file();
    }

    // 格式化日志行
    std::string line = std::format(
        "{} [{:5}] [{}] {}\n",
        format_timestamp(entry.timestamp),
        level_to_string(entry.level),
        format_thread_id(entry.thread_id),
        entry.message
    );

    output_file_ << line;
}

void AsyncLogger::flush() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 写入剩余数据
    while (!active_buffer_->empty()) {
        write_to_file(active_buffer_->front());
        active_buffer_->pop();
    }

    output_file_.flush();
}

void AsyncLogger::rotate_file() {
    output_file_.close();

    // 生成新文件名
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << base_filename_ << "." << std::put_time(std::localtime(&time), "%Y%m%d%H%M%S");

    std::string new_filename = ss.str();

    // 重命名文件
    std::rename(base_filename_.c_str(), new_filename.c_str());

    // 打开新文件
    output_file_.open(base_filename_, std::ios::app);
    stats_.file_rotations++;
}

std::string AsyncLogger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string AsyncLogger::format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string AsyncLogger::format_thread_id(std::thread::id tid) {
    std::stringstream ss;
    ss << tid;
    return ss.str();
}

AsyncLogger::~AsyncLogger() {
    shutdown();
}

