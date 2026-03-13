#include "downloader.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <format>

int main() {
    std::cout << "=== C++20 Multi-thread Downloader Demo ===\n";
    std::cout << "Features: Multi-thread, Resume, Speed control, Progress\n\n";

    MultiThreadDownloader downloader(4);  // 4 线程

    downloader.set_progress_callback(
        [](size_t downloaded, size_t total, double speed) {
            // 自定义进度处理
        }
    );

    // 模拟下载
    std::cout << "Starting simulated download...\n";
    downloader.download("http://example.com/largefile.zip", "download.zip");

    std::cout << "\nFinal status: " << downloader.get_status() << "\n";
    std::cout << "Total downloaded: " << (downloader.get_progress() > 0 ? 100 : 0) << " KB\n";
    std::cout << "Average speed: " << downloader.get_speed() / 1024 << " KB/s\n";

    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

