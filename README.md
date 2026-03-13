# High Performance C++20 Components

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)](https://github.com/kwjcyr/high_perf_cpp)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](https://github.com/kwjcyr/high_perf_cpp/blob/main/LICENSE)

🚀 **高性能 C++20 并发组件库** - 探索 C++ 高性能编程的边界

本项目是一系列高性能 C++20 组件的实现集合，涵盖内存管理、并发编程、系统编程等多个领域。每个组件都充分利用 C++20 新特性，追求极致的性能表现。

---

## 📚 目录

- [特性亮点](#-特性亮点)
- [组件列表](#-组件列表)
- [快速开始](#-快速开始)
- [组件详解](#-组件详解)
- [C++20 特性应用](#-c20-特性应用)
- [性能优化技巧](#-性能优化技巧)
- [项目结构](#-项目结构)
- [贡献与反馈](#-贡献与反馈)

---

## ✨ 特性亮点

- 🎯 **C++20 标准** - 全面采用 C++20 新特性（`std::format`、`std::atomic`、Concepts 等）
- ⚡ **高性能设计** - 无锁数据结构、内存对齐、缓存友好
- 🔒 **线程安全** - 正确的内存序选择、原子操作、锁优化
- 🛠️ **生产可用** - 每个组件都有完整的测试示例
- 📖 **学习价值** - 深入理解并发编程的深水区
- 🌍 **跨平台** - 支持 Linux 和 macOS

---

## 🧩 组件列表

| 组件 | 难度 | 状态 | 描述 |
|------|------|------|------|
| [内存池](#内存池) | ⭐⭐ | ✅ | 固定大小分配器，无锁快速路径 |
| [线程池](#线程池) | ⭐⭐⭐ | ✅ | 动态线程数，任务优先级，Future 模式 |
| [无锁队列](#无锁队列) | ⭐⭐⭐⭐ | ✅ | Michael & Scott 算法，解决 ABA 问题 |
| [异步日志](#异步日志库) | ⭐⭐⭐ | ✅ | 双缓冲设计，异步写入，日志轮转 |
| [Reactor 网络库](#reactor-网络库) | ⭐⭐⭐⭐ | ✅ | epoll/kqueue，事件驱动 |
| [多线程下载器](#多线程下载器) | ⭐⭐ | ✅ | 断点续传，并发下载，速度控制 |
| [数据库连接池](#数据库连接池) | ⭐⭐⭐ | ✅ | 连接生命周期管理，健康检查 |
| [双缓冲](#双缓冲) | ⭐⭐⭐ | ✅ | 无锁读取，写时复制，线程安全交换 |

---

## 🚀 快速开始

### 环境要求

- **编译器**: GCC 10+ / Clang 12+ / Apple Clang 14+
- **CMake**: 3.20+
- **标准**: C++20
- **系统**: Linux 或 macOS

### 编译步骤

```bash
# 克隆项目
git clone https://github.com/kwjcyr/high_perf_cpp.git
cd high_perf_cpp

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake ..

# 编译（并行构建）
make -j$(nproc)

# 运行示例（以线程池为例）
./thread_pool
```

### 单独编译某个组件

```bash
# 内存池
make memory_pool && ./memory_pool

# 无锁队列
make lock_free_queue && ./lock_free_queue

# 异步日志
make async_logger && ./async_logger

# Reactor 演示
make reactor_demo && ./reactor_demo
```

---

## 📖 组件详解

### 内存池 (Memory Pool)

**位置**: `memory_pool/`

**核心特性**:
- 固定大小分配器（16B ~ 1024B）
- 无锁快速路径（lock-free fast path）
- 缓存行对齐（64 字节）
- 线程安全的多分配器架构

**技术亮点**:
```cpp
// 无锁分配 - 使用 CAS 操作
BlockHeader* block = free_list_.load(std::memory_order_acquire);
while (block != nullptr) {
    if (free_list_.compare_exchange_weak(
            block, block->next,
            std::memory_order_acq_rel)) {
        return block;  // 分配成功
    }
}
```

**适用场景**: 高频小对象分配、实时系统、游戏引擎

---

### 线程池 (Thread Pool)

**位置**: `thread_pool/`

**核心特性**:
- 动态调整线程数量（1 ~ 64 线程）
- 任务优先级（LOW/NORMAL/HIGH/URGENT）
- Future/Promise 异步返回
- 优雅关闭机制

**技术亮点**:
```cpp
// C++20 spaceship 运算符用于优先级比较
auto operator<=>(const Task& other) const {
    if (priority != other.priority)
        return other.priority <=> priority;  // 高优先级优先
    return other.sequence <=> sequence;  // 同优先级按顺序
}

// 提交带返回值的任务
auto result = pool.submit_future([]() {
    return compute_heavy_task();
});
auto value = result.get();  // 获取结果
```

**适用场景**: 任务调度、并行计算、服务器后端

---

### 无锁队列 (Lock-Free Queue)

**位置**: `lock_free/`

**核心特性**:
- Michael & Scott 经典算法
- Tagged Pointer 解决 ABA 问题
- 正确的内存序选择（acquire/release）
- 无锁 enqueue/dequeue

**技术亮点**:
```cpp
// 带标签的指针 - 解决 ABA 问题
template<typename T>
class TaggedPointer {
    std::atomic<std::pair<T*, uint64_t>> data_;
    // tag 每次 CAS 时递增，防止 ABA
};

// 入队操作 - 完全无锁
void enqueue(const T& value) {
    auto new_node = new Node<T>(value);
    while (true) {
        auto tail = tail_.load(std::memory_order_acquire);
        auto next = tail->next.load(std::memory_order_acquire);
        // ... CAS 操作
    }
}
```

**适用场景**: 高并发消息队列、生产者 - 消费者模式、实时数据交换

---

### 异步日志库 (Async Logger)

**位置**: `async_logger/`

**核心特性**:
- 双缓冲设计，异步写入
- 不阻塞业务线程
- 支持日志轮转（10MB 自动轮转）
- C++20 `std::format` 高性能格式化

**技术亮点**:
```cpp
// 日志宏 - 使用 std::format
LOG_INFO("User {} logged in at {}", user_id, timestamp);
LOG_ERROR("Connection failed: error_code={}", error);

// 双缓冲切换
std::queue<LogEntry> buffer1_, buffer2_;
std::queue<LogEntry>* active_buffer_;
std::queue<LogEntry>* inactive_buffer_;
```

**日志级别**: DEBUG < INFO < WARNING < ERROR < FATAL

**适用场景**: 高性能服务日志、分布式系统、生产环境监控

---

### Reactor 网络库

**位置**: `reactor/`

**核心特性**:
- 跨平台支持（epoll for Linux, kqueue for macOS）
- 事件驱动架构
- 连接池管理
- 非阻塞 I/O

**技术亮点**:
```cpp
// 跨平台事件多路复用
#ifdef __APPLE__
    int kq_;  // kqueue
    std::vector<struct kevent> events_;
#else
    int epfd_;  // epoll
    std::vector<struct epoll_event> events_;
#endif

// 注册事件回调
reactor.register_event(fd, EventType::READ, []() {
    // 处理读事件
});
```

**⚠️ macOS 注意事项**: macOS 不支持 `SOCK_NONBLOCK`，需要特殊处理

**适用场景**: 高并发服务器、网络代理、实时通信

---

### 多线程下载器

**位置**: `downloader/`

**核心特性**:
- HTTP 断点续传
- 多线程并发下载
- 实时速度显示
- 进度条更新

**适用场景**: 文件下载工具、批量下载器

---

### 数据库连接池

**位置**: `db_pool/`

**核心特性**:
- 连接生命周期管理
- 健康检查机制
- 事务支持
- 连接复用

**适用场景**: 数据库密集应用、Web 服务后端

---

### 双缓冲 (Double Buffer)

**位置**: `double_buffer/`

**核心特性**:
- 无锁读取（lock-free read）
- 写时复制（copy-on-write）
- 线程安全的缓冲区交换
- 支持自定义数据类型

**扩展实现**:
- **RingBuffer**: 多生产者多消费者环形缓冲区
- **TripleBuffer**: 三缓冲，适用于高频写入场景

**技术亮点**:
```cpp
// 无锁读取
const T& read() const noexcept {
    size_t index = read_index_.load(std::memory_order_acquire);
    return *buffers_[index];
}

// 线程安全写入
void write(const T& value) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    size_t write_index = 1 - read_index_.load(std::memory_order_relaxed);
    *buffers_[write_index] = value;
    read_index_.store(write_index, std::memory_order_release);
}
```

**适用场景**: 实时数据交换、配置热更新、帧缓冲区

---

## 💡 C++20 特性应用

| 特性 | 应用场景 | 示例 |
|------|----------|------|
| `std::format` | 日志格式化、输出 | `std::format("Hello, {}", name)` |
| `std::atomic<T*>` | 无锁数据结构 | `std::atomic<Node*> head_` |
| `std::atomic<std::pair<T,U>>` | Tagged Pointer | C++20 原子操作复合类型 |
| 三路比较运算符 `<=>` | 优先级队列排序 | `auto operator<=>(const Task& other)` |
| `std::shared_ptr` | 资源管理 | 智能指针管理连接 |
| `std::function` | 回调封装 | 事件回调、任务函数 |
| `std::condition_variable` | 线程同步 | 生产者 - 消费者模式 |
| 内存序（memory_order） | 性能优化 | acquire/release/relaxed |

---

## 🔧 性能优化技巧

### 1. 内存序选择

```cpp
// 读操作 - acquire
auto value = atomic_var.load(std::memory_order_acquire);

// 写操作 - release
atomic_var.store(new_value, std::memory_order_release);

// 不需要同步 - relaxed
size_t count = counter_.load(std::memory_order_relaxed);
```

### 2. 缓存行对齐

```cpp
constexpr size_t CACHE_LINE_SIZE = 64;
// 避免 false sharing
struct alignas(64) CacheAlignedData {
    std::atomic<int> counter;
};
```

### 3. 无锁快速路径

```cpp
// 先尝试无锁 CAS
if (try_lock_free()) return;
// 失败再使用互斥锁
std::lock_guard<std::mutex> lock(mutex_);
```

### 4. 双缓冲设计

```cpp
// 写操作在后台缓冲区
write_to_inactive_buffer();
// 原子交换指针
std::atomic_exchange(&active_buffer, inactive_buffer);
// 读操作无锁访问
return active_buffer->read();
```

---

## 📁 项目结构

```
high_perf_cpp/
├── CMakeLists.txt              # 项目构建配置
├── README.md                   # 项目说明文档
├── README_IMPLEMENTATION.md    # 实现总结
├── memory_pool/                # 内存池
│   ├── memory_pool.h
│   └── main.cpp
├── thread_pool/                # 线程池
│   ├── thread_pool.h
│   ├── thread_pool.cpp
│   └── main.cpp
├── lock_free/                  # 无锁队列
│   ├── lock_free_queue.h
│   └── main.cpp
├── async_logger/               # 异步日志库
│   ├── logger.h
│   ├── logger.cpp
│   └── main.cpp
├── reactor/                    # Reactor 网络库
│   ├── reactor.h
│   ├── reactor.cpp
│   └── main.cpp
├── downloader/                 # 多线程下载器
│   ├── downloader.h
│   ├── downloader.cpp
│   └── main.cpp
├── db_pool/                    # 数据库连接池
│   ├── db_pool.h
│   ├── db_pool.cpp
│   └── main.cpp
├── double_buffer/              # 双缓冲
│   ├── double_buffer.h
│   └── main.cpp
└── leak_detector/              # 内存泄漏检测器（可选）
    └── main.cpp
```

---

## 🤝 贡献与反馈

欢迎贡献代码、报告问题或提出建议！

### 提交 Issue
- 发现 Bug？请提供复现步骤
- 有新想法？欢迎提出 Feature Request

### 提交 PR
1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

---

## 📝 学习路线建议

### 入门级（⭐⭐）
1. 内存池 - 理解内存管理基础
2. 多线程下载器 - 学习基本并发

### 进阶级（⭐⭐⭐）
3. 线程池 - 掌握任务调度
4. 异步日志 - 理解双缓冲设计
5. 数据库连接池 - 学习资源管理

### 高手级（⭐⭐⭐⭐）
6. 无锁队列 - 深入理解内存序
7. Reactor 网络库 - 掌握事件驱动
8. 双缓冲 - 学习无锁读写分离

---

## 📚 参考资料

- [C++20 标准文档](https://isocpp.org/std/the-standard)
- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition)
- [Lock-Free Programming](https://preshing.com/)
- [Intel Threading Building Blocks](https://github.com/intel/tbb)

---

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

---

## 👨‍💻 作者

**kwjcyr**

GitHub: [@kwjcyr](https://github.com/kwjcyr)

---

<div align="center">

**如果这个项目对你有帮助，请给一个 ⭐ Star 支持！**

[⬆ 返回顶部](#high-performance-c20-components)

</div>

