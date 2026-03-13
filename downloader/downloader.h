#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <chrono>
#include <functional>

// ============================================================================
// C++20 多线程下载工具
// 特性：
// 1. 断点续传
// 2. 多线程并发下载
// 3. 速度控制
// 4. 下载进度实时更新
// ============================================================================

struct DownloadTask {
    std::string url;
    std::string save_path;
    size_t total_size = 0;
    size_t downloaded_size = 0;
    std::vector<size_t> chunk_sizes;
};

class MultiThreadDownloader {
public:
    using ProgressCallback = std::function<void(size_t downloaded, size_t total, double speed)>;

    explicit MultiThreadDownloader(size_t max_threads = 4);
    ~MultiThreadDownloader();

    // 下载控制
    void download(const std::string& url, const std::string& save_path);
    void pause();
    void resume();
    void cancel();

    // 进度查询
    double get_progress() const;
    size_t get_speed() const;
    std::string get_status() const;

    // 回调设置
    void set_progress_callback(ProgressCallback callback);

    // 配置
    void set_max_threads(size_t threads);
    void set_speed_limit(size_t bytes_per_second);

private:
    struct ChunkInfo {
        size_t start;
        size_t end;
        size_t downloaded;
        std::string temp_file;
    };

    void download_chunk(ChunkInfo& chunk);
    void update_progress();
    std::string get_temp_file(const std::string& path, size_t chunk_id);
    void merge_chunks(const std::string& save_path);
    std::pair<size_t, bool> get_file_size(const std::string& url);

    std::vector<std::thread> workers_;
    std::vector<ChunkInfo> chunks_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<size_t> downloaded_{0};
    std::atomic<size_t> total_{0};
    std::atomic<size_t> speed_{0};

    size_t max_threads_ = 4;
    size_t speed_limit_ = 0;  // 0 = 无限制
    ProgressCallback progress_callback_;

    static constexpr size_t CHUNK_SIZE = 1024 * 1024;  // 1MB per chunk
};

