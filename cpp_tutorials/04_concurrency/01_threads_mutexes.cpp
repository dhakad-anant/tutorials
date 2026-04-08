/**
 * Module 04 — Lesson 1: Threads, Mutexes & Condition Variables
 *
 * WHY THIS MATTERS:
 *   Storage systems serve thousands of concurrent I/O requests. Understanding threads,
 *   synchronization primitives, and their pitfalls (deadlock, priority inversion, etc.)
 *   is non-negotiable for production C++.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -pthread -o threads 01_threads_mutexes.cpp
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <string>
#include <chrono>
#include <functional>
#include <numeric>
#include <cassert>
#include <map>

// ============================================================================
// Part 1: std::thread Basics
// ============================================================================

void demo_threads() {
    std::cout << "=== Thread Basics ===\n";

    // Launch a thread with a lambda
    std::thread t1([]() {
        std::cout << "  Thread 1 running (id=" << std::this_thread::get_id() << ")\n";
    });

    // Launch with a function + arguments
    auto worker = [](int id, const std::string& task) {
        std::cout << "  Worker " << id << " doing: " << task << "\n";
    };
    std::thread t2(worker, 2, "disk_read");
    std::thread t3(worker, 3, "compress");

    // MUST join (wait) or detach every thread
    t1.join();
    t2.join();
    t3.join();

    // Hardware concurrency
    std::cout << "  Hardware threads: " << std::thread::hardware_concurrency() << "\n";

    // DANGER: If you forget to join/detach, the destructor calls std::terminate()!
    // Use jthread (C++20) for automatic joining:
    {
        std::jthread jt([]() {
            std::cout << "  jthread: auto-joins on scope exit\n";
        });
        // No need to call jt.join() — destructor does it
    }
}

// ============================================================================
// Part 2: Mutex — Mutual Exclusion
// ============================================================================

class BankAccount {
public:
    explicit BankAccount(std::string name, int balance)
        : name_(std::move(name)), balance_(balance) {}

    void deposit(int amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        balance_ += amount;
    }

    bool withdraw(int amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (balance_ < amount) return false;
        balance_ -= amount;
        return true;
    }

    [[nodiscard]] int balance() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return balance_;
    }

    [[nodiscard]] const std::string& name() const { return name_; }

    // For transfers that lock BOTH accounts
    std::mutex& get_mutex() { return mutex_; }

private:
    std::string name_;
    int balance_;
    mutable std::mutex mutex_;
};

// DEADLOCK-FREE transfer using std::scoped_lock (locks multiple mutexes atomically)
void transfer(BankAccount& from, BankAccount& to, int amount) {
    // scoped_lock uses deadlock-avoidance algorithm internally
    std::scoped_lock lock(from.get_mutex(), to.get_mutex());
    // Both accounts are now locked — no deadlock possible
    from.withdraw(amount);
    to.deposit(amount);
}

void demo_mutex() {
    std::cout << "\n=== Mutex & Thread Safety ===\n";

    BankAccount account("Checking", 1000);
    std::vector<std::thread> threads;

    // 10 threads each deposit 100
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&account]() {
            for (int j = 0; j < 100; ++j) {
                account.deposit(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    std::cout << "  Final balance: " << account.balance() << " (expected 2000)\n";

    // Transfer demo
    BankAccount a("A", 500), b("B", 300);
    std::thread t1([&]() { transfer(a, b, 100); });
    std::thread t2([&]() { transfer(b, a, 50); });
    t1.join();
    t2.join();
    std::cout << "  A=" << a.balance() << " B=" << b.balance() << "\n";
}

// ============================================================================
// Part 3: Reader-Writer Lock (shared_mutex)
// ============================================================================

class ConfigStore {
public:
    std::string get(const std::string& key) const {
        std::shared_lock lock(mutex_);  // Multiple readers allowed simultaneously
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }

    void set(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);  // Exclusive access for writing
        data_[key] = value;
    }

private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::string> data_;
};

void demo_rwlock() {
    std::cout << "\n=== Reader-Writer Lock ===\n";

    ConfigStore config;
    config.set("block_size", "4096");
    config.set("compression", "lz4");

    // Multiple readers can run concurrently
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&config, i]() {
            auto val = config.get("block_size");
            std::cout << "  Reader " << i << ": block_size=" << val << "\n";
        });
    }
    for (auto& t : readers) t.join();
}

// ============================================================================
// Part 4: Condition Variables — Thread Signaling
// ============================================================================

template <typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        // Notify AFTER releasing the lock (more efficient)
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait until queue is non-empty
        // The predicate guards against spurious wakeups
        cv_.wait(lock, [this]() { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // Non-blocking try
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};

void demo_condition_variable() {
    std::cout << "\n=== Condition Variable: Producer-Consumer ===\n";

    ThreadSafeQueue<std::string> work_queue;

    // Consumer thread
    std::jthread consumer([&work_queue](std::stop_token st) {
        while (!st.stop_requested()) {
            std::string item;
            if (work_queue.try_pop(item)) {
                std::cout << "  Consumed: " << item << "\n";
            } else {
                std::this_thread::yield();
            }
        }
        // Drain remaining items
        std::string item;
        while (work_queue.try_pop(item)) {
            std::cout << "  Consumed (drain): " << item << "\n";
        }
    });

    // Producer
    for (int i = 0; i < 5; ++i) {
        work_queue.push("IO_Request_" + std::to_string(i));
    }

    // Small delay to let consumer process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    consumer.request_stop();
}

// ============================================================================
// Part 5: Common Pitfalls
// ============================================================================

/*
 * DEADLOCK: Thread A holds lock 1, waits for lock 2.
 *           Thread B holds lock 2, waits for lock 1.
 *   FIX: Use std::scoped_lock. Or always lock in the same global order.
 *
 * DATA RACE: Two threads access same memory, at least one writes, no sync.
 *   FIX: Guard with mutex. Or use std::atomic. Or redesign to avoid sharing.
 *   DETECTION: Compile with -fsanitize=thread (ThreadSanitizer).
 *
 * PRIORITY INVERSION: Low-priority thread holds lock, high-priority waits.
 *   FIX: Priority inheritance (OS-level), or minimize lock hold time.
 *
 * LOCK CONVOY: Many threads wake up but only one can acquire the lock.
 *   FIX: notify_one() instead of notify_all() when possible.
 *
 * SPURIOUS WAKEUP: condition_variable::wait() can return without notification.
 *   FIX: ALWAYS use the predicate form: cv.wait(lock, [&]{ return condition; });
 */

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_threads();
    demo_mutex();
    demo_rwlock();
    demo_condition_variable();

    return 0;
}

// ============================================================================
// EXERCISES:
//
// 1. Implement a thread-safe hash map with per-bucket locking (striped lock).
//    Benchmark against a single-mutex version with 8 threads.
//
// 2. Write a `Barrier` class: N threads call arrive_and_wait(). All block until
//    all N have arrived, then all proceed. (C++20 has std::barrier — try both.)
//
// 3. Implement a "dining philosophers" solution using std::scoped_lock.
//
// 4. Write a producer-consumer where 3 producers and 2 consumers share a
//    bounded queue (max 10 items). Use condition variables for both full/empty.
// ============================================================================
