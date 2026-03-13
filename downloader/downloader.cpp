#include "downloader.h"
#include <iostream>
#include <format>

MultiThreadDownloader::MultiThreadDownloader(size_t max_threads)
    : max_threads_(max_threads) {}

MultiThreadDownloader::~MultiThreadDownloader() {
    cancel();
}

void MultiThreadDownloader::download(const std::string& url, const std::string& save_path) {
    std::cout << "Starting download:\n";
    std::cout << "  URL: " << url << "\n";
    std::cout << "  Save to: " << save_path << "\n";
    std::cout << "  Threads: " << max_threads_ << "\n\n";

    // 模拟下载过程
    total_ = 100 * 1024 * 1024;  // 模拟 100MB 文件

    const size_t CHUNK_SIZE = 10 * 1024 * 1024;  // 10MB per chunk
    size_t total_size = total_.load();
    size_t num_chunks = (total_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    for (size_t i = 0; i < num_chunks; ++i) {
        ChunkInfo chunk;
        chunk.start = i * CHUNK_SIZE;
        chunk.end = std::min((i + 1) * CHUNK_SIZE, total_size) - 1;
        chunk.downloaded = 0;
        chunk.temp_file = get_temp_file(save_path, i);
        chunks_.push_back(chunk);
    }

    running_.store(true);
    paused_.store(false);

    size_t actual_threads = std::min(max_threads_, chunks_.size());

    for (size_t i = 0; i < actual_threads; ++i) {
        workers_.emplace_back([this, i, actual_threads]() mutable {
            size_t idx = i;
            while (idx < chunks_.size() && running_.load()) {
                if (paused_.load()) {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() { return !paused_.load() || !running_.load(); });
                }

                if (!running_.load()) break;

                download_chunk(chunks_[idx]);
                idx += actual_threads;
            }
        });
    }

    // 进度更新线程
    std::thread progress_thread([this]() {
        update_progress();
    });

    // 等待下载完成
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }

    running_.store(false);
    cv_.notify_all();

    if (progress_thread.joinable()) progress_thread.join();

    if (!paused_.load() && downloaded_ >= total_) {
        merge_chunks(save_path);
    }
}

void MultiThreadDownloader::download_chunk(ChunkInfo& chunk) {
    // 模拟下载
    const size_t BLOCK_SIZE = 1024 * 1024;  // 1MB blocks
    auto start_time = std::chrono::steady_clock::now();

    for (size_t pos = chunk.downloaded; pos < (chunk.end - chunk.start + 1); pos += BLOCK_SIZE) {
        if (!running_.load()) break;
        if (paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 模拟网络延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        size_t block_size = std::min(BLOCK_SIZE, chunk.end - chunk.start + 1 - pos);
        chunk.downloaded += block_size;
        downloaded_.fetch_add(block_size);

        // 速度限制
        if (speed_limit_ > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            size_t expected_time = (block_size * 1000) / speed_limit_;
            if (elapsed < expected_time) {
                std::this_thread::sleep_for(std::chrono::milliseconds(expected_time - elapsed));
            }
        }
    }
}

void MultiThreadDownloader::update_progress() {
    auto last_update = std::chrono::steady_clock::now();
    size_t last_downloaded = 0;

    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        size_t current = downloaded_.load();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();

        if (elapsed > 0) {
            speed_.store((current - last_downloaded) * 1000 / elapsed);
        }

        last_downloaded = current;
        last_update = now;

        double progress = (current * 100.0) / total_.load();

        if (progress_callback_) {
            progress_callback_(current, total_.load(), speed_.load());
        }

        std::cout << "\rProgress: " << std::format("{:.1f}", progress)
                  << "% | Speed: " << speed_.load() / 1024 << " KB/s";
        std::cout.flush();
    }
}

void MultiThreadDownloader::pause() {
    paused_.store(true);
    std::cout << "\nDownload paused\n";
}

void MultiThreadDownloader::resume() {
    if (paused_.load()) {
        paused_.store(false);
        cv_.notify_all();
        std::cout << "Download resumed\n";
    }
}

void MultiThreadDownloader::cancel() {
    running_.store(false);
    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

double MultiThreadDownloader::get_progress() const {
    size_t current = downloaded_.load();
    size_t total = total_.load();
    return total > 0 ? (current * 100.0) / total : 0.0;
}

size_t MultiThreadDownloader::get_speed() const {
    return speed_.load();
}

std::string MultiThreadDownloader::get_status() const {
    if (paused_.load()) return "Paused";
    if (running_.load()) return "Downloading";
    return downloaded_ >= total_ ? "Completed" : "Cancelled";
}

void MultiThreadDownloader::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

void MultiThreadDownloader::set_max_threads(size_t threads) {
    max_threads_ = threads;
}

void MultiThreadDownloader::set_speed_limit(size_t bytes_per_second) {
    speed_limit_ = bytes_per_second;
}

std::string MultiThreadDownloader::get_temp_file(const std::string& path, size_t chunk_id) {
    return path + ".chunk" + std::to_string(chunk_id);
}

void MultiThreadDownloader::merge_chunks(const std::string& save_path) {
    std::cout << "\nMerging chunks...\n";

    std::ofstream output(save_path, std::ios::binary);

    for (const auto& chunk : chunks_) {
        std::ifstream input(chunk.temp_file, std::ios::binary);
        output << input.rdbuf();
    }

    output.close();
    std::cout << "Download complete: " << save_path << "\n";
}

std::pair<size_t, bool> MultiThreadDownloader::get_file_size(const std::string& url) {
    // 实际实现需要 HTTP HEAD 请求
    // 这里返回模拟值
    return {total_.load(), true};
}

