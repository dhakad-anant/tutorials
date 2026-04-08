/**
 * Module 04 — Lesson 2: Atomics & Lock-Free Programming
 *
 * WHY THIS MATTERS:
 *   In high-throughput storage systems, mutex contention is a bottleneck. Atomics
 *   provide synchronization WITHOUT locks — critical for counters, flags, and
 *   lock-free data structures on the hot path.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -pthread -O2 -o atomics 02_atomics_lockfree.cpp
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <array>
#include <optional>
#include <mutex>

// ============================================================================
// Part 1: std::atomic Basics
// ============================================================================

void demo_atomic_basics() {
    std::cout << "=== Atomic Basics ===\n";

    std::atomic<int> counter{0};

    // Atomic operations — no mutex needed
    counter.store(42);                    // write
    int val = counter.load();             // read
    counter.fetch_add(10);                // read-modify-write
    counter.fetch_sub(5);                 // read-modify-write

    std::cout << "  counter = " << counter.load() << "\n";  // 47

    // Compare-and-swap (CAS) — the fundamental lock-free primitive
    int expected = 47;
    bool success = counter.compare_exchange_strong(expected, 100);
    // If counter == expected (47), set counter = 100, return true
    // If counter != expected, set expected = actual value, return false
    std::cout << "  CAS success: " << std::boolalpha << success
              << " counter=" << counter.load() << "\n";

    // Exchange: atomically swap
    int old = counter.exchange(200);
    std::cout << "  exchanged " << old << " → " << counter.load() << "\n";
}

// ============================================================================
// Part 2: Atomic Counter — Benchmark vs Mutex
// ============================================================================

void demo_atomic_vs_mutex() {
    std::cout << "\n=== Atomic vs Mutex Performance ===\n";

    constexpr int N_THREADS = 8;
    constexpr int N_OPS = 1'000'000;

    // Atomic counter
    {
        std::atomic<long long> counter{0};
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int i = 0; i < N_THREADS; ++i) {
            threads.emplace_back([&counter]() {
                for (int j = 0; j < N_OPS; ++j) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& t : threads) t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  Atomic:  " << counter.load() << " in " << ms.count() << " ms\n";
    }

    // Mutex counter
    {
        long long counter = 0;
        std::mutex mutex;
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int i = 0; i < N_THREADS; ++i) {
            threads.emplace_back([&counter, &mutex]() {
                for (int j = 0; j < N_OPS; ++j) {
                    std::lock_guard lock(mutex);
                    ++counter;
                }
            });
        }
        for (auto& t : threads) t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  Mutex:   " << counter << " in " << ms.count() << " ms\n";
    }
}

// ============================================================================
// Part 3: Memory Ordering — The Hard Part
// ============================================================================

/*
 * CPUs and compilers REORDER instructions for performance. Memory ordering
 * constraints tell them what reordering is allowed.
 *
 * memory_order_relaxed:
 *   - Only guarantees atomicity. No ordering constraints.
 *   - USE FOR: counters, statistics — where you don't care about ordering.
 *
 * memory_order_acquire:
 *   - Reads: no memory reads/writes in THIS thread can be reordered BEFORE this load.
 *   - Like a "fence" that says "everything after this sees the latest data."
 *
 * memory_order_release:
 *   - Writes: no memory reads/writes in THIS thread can be reordered AFTER this store.
 *   - Pairs with acquire: publish data, then set a flag.
 *
 * memory_order_seq_cst (default):
 *   - Sequential consistency. All threads see operations in the same total order.
 *   - Safest but slowest. Use when you're unsure.
 *
 * RULE OF THUMB:
 *   - Start with seq_cst (default). Profile. Only relax if needed AND you understand why.
 *   - acquire/release is the most common "optimized" pattern.
 */

// Producer-consumer with acquire/release
std::atomic<bool> data_ready{false};
int shared_data = 0;

void producer() {
    shared_data = 42;  // NON-atomic write
    data_ready.store(true, std::memory_order_release);
    // Release guarantees: shared_data = 42 is visible to anyone who sees data_ready == true
}

void consumer() {
    while (!data_ready.load(std::memory_order_acquire)) {
        // spin-wait
    }
    // Acquire guarantees: we see all writes that happened before the release
    assert(shared_data == 42);  // guaranteed to be true
    std::cout << "  Consumer saw: " << shared_data << "\n";
}

void demo_memory_ordering() {
    std::cout << "\n=== Memory Ordering: Acquire/Release ===\n";
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
}

// ============================================================================
// Part 4: Spinlock — Simple Lock-Free Mutex
// ============================================================================

class SpinLock {
public:
    void lock() {
        // Try to set flag from false→true. Spin until successful.
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Busy-wait. In production, add:
            // - Exponential backoff
            // - std::this_thread::yield() after N spins
            // - Pause instruction (__builtin_ia32_pause on x86)
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

void demo_spinlock() {
    std::cout << "\n=== Spinlock ===\n";

    SpinLock spin;
    int counter = 0;

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100'000; ++j) {
                spin.lock();
                ++counter;
                spin.unlock();
            }
        });
    }
    for (auto& t : threads) t.join();
    std::cout << "  Counter: " << counter << " (expected 400000)\n";
}

// ============================================================================
// Part 5: Lock-Free Stack (Simple Example)
// ============================================================================

template <typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
    };

public:
    void push(T value) {
        auto* node = new Node{std::move(value), nullptr};
        // CAS loop: retry until we successfully link at the top
        node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(
            node->next, node,
            std::memory_order_release,
            std::memory_order_relaxed)) {
            // CAS failed: node->next updated to current head, retry
        }
    }

    std::optional<T> pop() {
        Node* old_head = head_.load(std::memory_order_acquire);
        while (old_head && !head_.compare_exchange_weak(
            old_head, old_head->next,
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
            // retry
        }
        if (!old_head) return std::nullopt;

        T value = std::move(old_head->data);
        delete old_head;  // WARNING: ABA problem! See note below.
        return value;
    }

    ~LockFreeStack() {
        while (pop().has_value()) {}
    }

private:
    std::atomic<Node*> head_{nullptr};
};

void demo_lockfree_stack() {
    std::cout << "\n=== Lock-Free Stack ===\n";

    LockFreeStack<int> stack;

    // Push from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&stack, i]() {
            for (int j = 0; j < 1000; ++j) {
                stack.push(i * 1000 + j);
            }
        });
    }
    for (auto& t : threads) t.join();

    // Pop all
    int count = 0;
    while (stack.pop().has_value()) ++count;
    std::cout << "  Popped " << count << " items (expected 4000)\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_atomic_basics();
    demo_atomic_vs_mutex();
    demo_memory_ordering();
    demo_spinlock();
    demo_lockfree_stack();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. std::atomic provides lock-free thread-safe operations for simple types.
// 2. CAS (compare_exchange) is the building block of all lock-free algorithms.
// 3. Memory ordering: start with seq_cst, relax ONLY when profiling shows need.
// 4. acquire/release is the most common optimized pattern for publish/consume.
// 5. Lock-free != wait-free. Lock-free means "some thread always makes progress."
// 6. The ABA problem: a lock-free pop() can fail if a node is freed and
//    reallocated at the same address. Fix: hazard pointers or epoch-based reclamation.
// 7. Spinlocks are useful for VERY short critical sections (<1µs).
//    For anything longer, use a mutex (which yields the CPU to the OS scheduler).
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Add exponential backoff to SpinLock (yield after N spins, then sleep).
//    Benchmark against std::mutex under high contention.
//
// 2. Implement an atomic MPSC (multi-producer, single-consumer) queue.
//
// 3. Write a lock-free counter that supports increment, decrement, and
//    compare_exchange_add (add only if current value < max). Use CAS loop.
//
// 4. Research the ABA problem. Explain why the LockFreeStack above is
//    vulnerable. Sketch a fix using a version counter (tagged pointer).
// ============================================================================
