/**
 * Module 02 — Lesson 3: Custom Allocators & Memory Pools
 *
 * WHY THIS MATTERS:
 *   In high-performance storage systems, malloc/free is too slow and unpredictable
 *   for the hot path. Custom allocators give you deterministic allocation times,
 *   better cache locality, and zero fragmentation.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -O2 -o allocators 03_custom_allocators.cpp
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <array>
#include <cassert>
#include <list>
#include <new>

// ============================================================================
// Part 1: Arena (Bump) Allocator
// ============================================================================

// Simplest possible allocator: allocate by bumping a pointer.
// Free is a no-op. Reset frees everything at once.
// Perfect for: request handling, frame-based work, temporary computations.

class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t capacity)
        : capacity_(capacity),
          buffer_(std::make_unique<char[]>(capacity)),
          offset_(0) {}

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align the current offset
        size_t aligned_offset = (offset_ + alignment - 1) & ~(alignment - 1);

        if (aligned_offset + size > capacity_) {
            throw std::bad_alloc();
        }

        void* ptr = buffer_.get() + aligned_offset;
        offset_ = aligned_offset + size;
        ++allocation_count_;
        return ptr;
    }

    // Construct an object in the arena
    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    // Free everything at once — O(1)!
    void reset() {
        offset_ = 0;
        allocation_count_ = 0;
    }

    [[nodiscard]] size_t used() const { return offset_; }
    [[nodiscard]] size_t remaining() const { return capacity_ - offset_; }
    [[nodiscard]] size_t allocation_count() const { return allocation_count_; }

private:
    size_t capacity_;
    std::unique_ptr<char[]> buffer_;
    size_t offset_;
    size_t allocation_count_ = 0;
};

void demo_arena() {
    std::cout << "=== Arena Allocator ===\n";

    ArenaAllocator arena(4096);

    // Allocate various objects
    int* a = arena.create<int>(42);
    double* b = arena.create<double>(3.14);

    struct Point { float x, y, z; };
    Point* p = arena.create<Point>(Point{1.0f, 2.0f, 3.0f});

    std::cout << "  *a = " << *a << "\n";
    std::cout << "  *b = " << *b << "\n";
    std::cout << "  p = (" << p->x << ", " << p->y << ", " << p->z << ")\n";
    std::cout << "  Used: " << arena.used() << " / 4096 bytes\n";
    std::cout << "  Allocations: " << arena.allocation_count() << "\n";

    // Reset — all pointers are now invalid, but memory is reusable
    arena.reset();
    std::cout << "  After reset: " << arena.used() << " bytes used\n";
}

// ============================================================================
// Part 2: Pool (Freelist) Allocator
// ============================================================================

// Fixed-size block allocator. All allocations are the same size.
// Free returns blocks to a freelist for reuse.
// Perfect for: objects created/destroyed frequently (network packets, I/O requests).

template <size_t BlockSize, size_t BlockCount>
class PoolAllocator {
    static_assert(BlockSize >= sizeof(void*), "Block must fit a pointer");

public:
    PoolAllocator() {
        // Initialize freelist: each block points to the next
        for (size_t i = 0; i < BlockCount - 1; ++i) {
            auto* block = reinterpret_cast<FreeNode*>(&storage_[i * BlockSize]);
            block->next = reinterpret_cast<FreeNode*>(&storage_[(i + 1) * BlockSize]);
        }
        auto* last = reinterpret_cast<FreeNode*>(&storage_[(BlockCount - 1) * BlockSize]);
        last->next = nullptr;
        freelist_ = reinterpret_cast<FreeNode*>(&storage_[0]);
    }

    void* allocate() {
        if (!freelist_) {
            throw std::bad_alloc();
        }
        void* block = freelist_;
        freelist_ = freelist_->next;
        ++in_use_;
        return block;
    }

    void deallocate(void* ptr) {
        auto* node = static_cast<FreeNode*>(ptr);
        node->next = freelist_;
        freelist_ = node;
        --in_use_;
    }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        static_assert(sizeof(T) <= BlockSize, "Object too large for pool block");
        void* mem = allocate();
        return new (mem) T(std::forward<Args>(args)...);
    }

    template <typename T>
    void destroy(T* ptr) {
        ptr->~T();
        deallocate(ptr);
    }

    [[nodiscard]] size_t in_use() const { return in_use_; }
    [[nodiscard]] size_t available() const { return BlockCount - in_use_; }

private:
    struct FreeNode {
        FreeNode* next;
    };

    alignas(std::max_align_t) char storage_[BlockSize * BlockCount];
    FreeNode* freelist_ = nullptr;
    size_t in_use_ = 0;
};

void demo_pool() {
    std::cout << "\n=== Pool Allocator ===\n";

    struct IORequest {
        uint64_t offset;
        uint32_t size;
        uint8_t  flags;
    };

    PoolAllocator<sizeof(IORequest), 1024> pool;

    // Allocate some requests
    auto* req1 = pool.create<IORequest>(IORequest{0x1000, 4096, 0x01});
    auto* req2 = pool.create<IORequest>(IORequest{0x2000, 8192, 0x02});
    auto* req3 = pool.create<IORequest>(IORequest{0x3000, 4096, 0x01});

    std::cout << "  In use: " << pool.in_use() << " / 1024\n";

    // Free one — goes back to freelist
    pool.destroy(req2);
    std::cout << "  After free: " << pool.in_use() << " in use\n";

    // Allocate again — reuses the freed block (O(1)!)
    auto* req4 = pool.create<IORequest>(IORequest{0x4000, 4096, 0x00});
    std::cout << "  Allocated req4 at " << req4 << "\n";
    std::cout << "  req2 was at       " << req2 << " (likely same address — reused!)\n";

    pool.destroy(req1);
    pool.destroy(req3);
    pool.destroy(req4);
    std::cout << "  Final in use: " << pool.in_use() << "\n";
}

// ============================================================================
// Part 3: Benchmark — Custom Allocator vs malloc
// ============================================================================

void demo_benchmark() {
    std::cout << "\n=== Benchmark: Pool vs malloc ===\n";

    constexpr int N = 100'000;
    struct Block { char data[64]; };

    // Benchmark malloc/free
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<Block*> ptrs(N);
        for (int i = 0; i < N; ++i) {
            ptrs[i] = static_cast<Block*>(std::malloc(sizeof(Block)));
        }
        for (int i = 0; i < N; ++i) {
            std::free(ptrs[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  malloc/free:    " << us.count() << " µs\n";
    }

    // Benchmark pool allocator
    {
        PoolAllocator<sizeof(Block), 100'000> pool;
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<void*> ptrs(N);
        for (int i = 0; i < N; ++i) {
            ptrs[i] = pool.allocate();
        }
        for (int i = 0; i < N; ++i) {
            pool.deallocate(ptrs[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  Pool allocator: " << us.count() << " µs\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_arena();
    demo_pool();
    demo_benchmark();
    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Arena allocator: bump pointer, bulk free. Perfect for request-scoped work.
// 2. Pool allocator: fixed-size blocks, freelist reuse. Perfect for frequent
//    alloc/dealloc of same-sized objects.
// 3. Custom allocators beat malloc because they avoid:
//    - Global lock contention (malloc has a lock)
//    - Fragmentation (pool is fixed-size, arena is linear)
//    - Metadata overhead (malloc stores size per allocation)
// 4. Production usage: network packet pools, I/O request pools, scratch memory.
// 5. C++ STL containers accept custom allocators as template parameters.
//    std::vector<int, MyAllocator<int>> — but this is an advanced topic.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Add a `create_array<T>(size_t n)` method to ArenaAllocator that allocates
//    an array of n elements. Use it to allocate 100 ints and verify they work.
//
// 2. Make the PoolAllocator thread-safe using std::mutex. Benchmark the overhead.
//    Then try a lock-free version using std::atomic<FreeNode*>.
//
// 3. Implement a StackAllocator that supports LIFO deallocation:
//    - allocate() bumps a pointer (like arena)
//    - deallocate() must be called in reverse order (asserts if not)
//
// 4. Write a STL-compatible allocator (Allocator<T>) that wraps ArenaAllocator.
//    Use it with std::vector<int, Allocator<int>>.
// ============================================================================
