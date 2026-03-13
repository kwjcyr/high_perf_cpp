cmake_minimum_required(VERSION 3.20)
project(cpp20_high_performance)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 内存池
add_executable(memory_pool
    memory_pool/main.cpp
)

# 内存泄漏检测器（需要 GCC 支持 stacktrace）
# add_executable(leak_detector
#     leak_detector/main.cpp
# )

# 线程池
add_executable(thread_pool
    thread_pool/main.cpp
    thread_pool/thread_pool.cpp
)

# 无锁队列
add_executable(lock_free_queue
    lock_free/main.cpp
)

# 异步日志
add_executable(async_logger
    async_logger/main.cpp
    async_logger/logger.cpp
)

# Reactor 网络库
add_executable(reactor_demo
    reactor/main.cpp
    reactor/reactor.cpp
)

# 多线程下载器
add_executable(downloader
    downloader/main.cpp
    downloader/downloader.cpp
)

# 数据库连接池
add_executable(db_pool
    db_pool/main.cpp
    db_pool/db_pool.cpp
)

# 双缓冲
add_executable(double_buffer
    double_buffer/main.cpp
)

