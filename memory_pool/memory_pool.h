#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <algorithm>

// ============================================================================
// C++20 简化版内存池
// ============================================================================

constexpr size_t CACHE_LINE_SIZE = 64;
constexpr size_t MAX_BLOCK_SIZE = 1024;

struct BlockHeader {
    BlockHeader* next;
    size_t size;
    bool is_used;
};

template<size_t BlockSize>
class FixedSizeAllocator {
public:
    FixedSizeAllocator() : free_list_(nullptr) {
        allocate_chunk();
    }

    void* allocate() {
        BlockHeader* block = free_list_.load(std::memory_order_acquire);

        while (block != nullptr) {
            if (free_list_.compare_exchange_weak(
                    block,
                    block->next,
                    std::memory_order_acq_rel)) {
                return block;
            }
            block = free_list_.load(std::memory_order_acquire);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        allocate_chunk();
        return allocate();
    }

    void deallocate(void* ptr) {
        if (!ptr) return;

        BlockHeader* block = static_cast<BlockHeader*>(ptr);
        block->is_used = false;

        BlockHeader* old_head = free_list_.load(std::memory_order_relaxed);
        do {
            block->next = old_head;
        } while (!free_list_.compare_exchange_weak(
            old_head,
            block,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    size_t get_usage() const {
        size_t count = 0;
        BlockHeader* block = free_list_.load(std::memory_order_relaxed);
        while (block) {
            count++;
            block = block->next;
        }
        return count;
    }

private:
    std::atomic<BlockHeader*> free_list_;
    std::mutex mutex_;
    std::vector<std::unique_ptr<char[]>> chunks_;

    void allocate_chunk() {
        auto chunk = std::make_unique<char[]>(64 * 1024);
        char* memory = chunk.get();

        for (size_t i = 0; i < 64 * 1024; i += BlockSize + sizeof(BlockHeader)) {
            BlockHeader* block = reinterpret_cast<BlockHeader*>(memory + i);
            block->size = BlockSize;
            block->is_used = false;
            block->next = free_list_.load(std::memory_order_relaxed);
            free_list_.store(block, std::memory_order_relaxed);
        }

        chunks_.push_back(std::move(chunk));
    }
};

class MemoryPool {
public:
    static MemoryPool& instance() {
        static MemoryPool pool;
        return pool;
    }

    void* allocate(size_t size) {
        if (size == 0) return nullptr;
        if (size > MAX_BLOCK_SIZE) {
            return ::operator new(size);
        }

        if (size <= 16) return alloc16_.allocate();
        if (size <= 32) return alloc32_.allocate();
        if (size <= 64) return alloc64_.allocate();
        if (size <= 128) return alloc128_.allocate();
        if (size <= 256) return alloc256_.allocate();
        if (size <= 512) return alloc512_.allocate();
        return alloc1024_.allocate();
    }

    void deallocate(void* ptr, size_t size) {
        if (!ptr) return;
        if (size > MAX_BLOCK_SIZE) {
            ::operator delete(ptr);
            return;
        }

        if (size <= 16) return alloc16_.deallocate(ptr);
        if (size <= 32) return alloc32_.deallocate(ptr);
        if (size <= 64) return alloc64_.deallocate(ptr);
        if (size <= 128) return alloc128_.deallocate(ptr);
        if (size <= 256) return alloc256_.deallocate(ptr);
        if (size <= 512) return alloc512_.deallocate(ptr);
        return alloc1024_.deallocate(ptr);
    }

private:
    FixedSizeAllocator<16> alloc16_;
    FixedSizeAllocator<32> alloc32_;
    FixedSizeAllocator<64> alloc64_;
    FixedSizeAllocator<128> alloc128_;
    FixedSizeAllocator<256> alloc256_;
    FixedSizeAllocator<512> alloc512_;
    FixedSizeAllocator<1024> alloc1024_;

    size_t size_to_index(size_t size) {
        if (size <= 16) return 0;
        if (size <= 32) return 1;
        if (size <= 64) return 2;
        if (size <= 128) return 3;
        if (size <= 256) return 4;
        if (size <= 512) return 5;
        return 6;
    }
};

#define POOL_NEW(type) new (MemoryPool::instance().allocate(sizeof(type))) type
#define POOL_DELETE(ptr) MemoryPool::instance().deallocate(ptr, sizeof(*ptr))

