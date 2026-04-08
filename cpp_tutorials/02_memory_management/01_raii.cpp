/**
 * Module 02 — Lesson 1: RAII (Resource Acquisition Is Initialization)
 *
 * WHY THIS MATTERS:
 *   RAII is THE pattern of C++. It's how you ensure correctness in the face of
 *   exceptions, early returns, and complex control flow. Every mutex lock, every
 *   file handle, every network connection — all managed via RAII in production code.
 *
 * CORE IDEA:
 *   - Acquire a resource in the constructor
 *   - Release it in the destructor
 *   - The compiler guarantees the destructor runs at scope exit (even on exception)
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o raii 01_raii.cpp
 */

#include <iostream>
#include <fstream>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <utility>

// ============================================================================
// Part 1: RAII for Mutex Locking
// ============================================================================

class ThreadSafeCounter {
public:
    void increment() {
        // std::lock_guard IS RAII for mutexes
        // Constructor: locks the mutex
        // Destructor:  unlocks the mutex (even if exception is thrown)
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
    }

    void add(int n) {
        // std::unique_lock is RAII too, but more flexible (can unlock early)
        std::unique_lock<std::mutex> lock(mutex_);
        count_ += n;
        lock.unlock();  // can unlock before scope ends if needed
        // ... do work that doesn't need the lock ...
    }

    [[nodiscard]] int get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    int count_ = 0;
};

// ============================================================================
// Part 2: RAII for File Handles
// ============================================================================

// std::fstream is already RAII — closes file in destructor.
// But what about custom resources?

class MappedFile {
public:
    explicit MappedFile(const std::string& path)
        : path_(path) {
        std::cout << "  [MappedFile] Opening: " << path_ << "\n";
        // In real code: open(), mmap(), etc.
        data_ = new char[4096];  // simulated
        size_ = 4096;
    }

    ~MappedFile() {
        std::cout << "  [MappedFile] Closing: " << path_ << "\n";
        // In real code: munmap(), close()
        delete[] data_;
    }

    // Rule of Five: since we manage a resource, define all five
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    MappedFile(MappedFile&& other) noexcept
        : path_(std::move(other.path_)),
          data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)) {
        std::cout << "  [MappedFile] Moved: " << path_ << "\n";
    }

    MappedFile& operator=(MappedFile&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            path_ = std::move(other.path_);
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] const char* data() const { return data_; }

private:
    std::string path_;
    char*       data_ = nullptr;
    size_t      size_ = 0;
};

// ============================================================================
// Part 3: RAII Scope Guard — Generic Cleanup
// ============================================================================

// A scope guard runs a cleanup function at scope exit — perfect for C APIs

template <typename Func>
class ScopeGuard {
public:
    explicit ScopeGuard(Func&& f) : func_(std::move(f)), active_(true) {}

    ~ScopeGuard() {
        if (active_) {
            func_();  // execute cleanup on scope exit
        }
    }

    // Dismiss the guard (e.g., if commit succeeded, don't rollback)
    void dismiss() { active_ = false; }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&& other) noexcept
        : func_(std::move(other.func_)), active_(other.active_) {
        other.active_ = false;
    }

private:
    Func func_;
    bool active_;
};

// Factory function for type deduction
template <typename Func>
[[nodiscard]] ScopeGuard<Func> make_scope_guard(Func&& f) {
    return ScopeGuard<Func>(std::forward<Func>(f));
}

void demo_scope_guard() {
    std::cout << "\n=== Scope Guard Demo ===\n";

    // Simulated transaction
    std::cout << "  Starting transaction...\n";
    auto rollback_guard = make_scope_guard([]() {
        std::cout << "  ROLLING BACK transaction\n";
    });

    // ... do some work ...
    bool success = true;  // change to false to see rollback

    if (success) {
        std::cout << "  Committing transaction\n";
        rollback_guard.dismiss();  // don't rollback — we committed
    }
    std::cout << "  (scope exit)\n";
}

// ============================================================================
// Part 4: RAII Timer — Measure Scope Duration
// ============================================================================

class ScopeTimer {
public:
    explicit ScopeTimer(std::string name)
        : name_(std::move(name)),
          start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopeTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        std::cout << "  [Timer] " << name_ << ": " << us.count() << " µs\n";
    }

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

void demo_scope_timer() {
    std::cout << "\n=== Scope Timer Demo ===\n";
    {
        ScopeTimer timer("vector fill");
        std::vector<int> v(1'000'000);
        for (int i = 0; i < 1'000'000; ++i) v[i] = i;
    }
    {
        ScopeTimer timer("string concat");
        std::string s;
        for (int i = 0; i < 10'000; ++i) s += "x";
    }
}

// ============================================================================
// Part 5: Exception Safety Guarantees
// ============================================================================

/*
 * RAII is the foundation of exception safety. There are 3 levels:
 *
 * 1. BASIC GUARANTEE (minimum acceptable):
 *    - No leaks. Invariants preserved. Object is in SOME valid state.
 *    - Achieved by: RAII everywhere.
 *
 * 2. STRONG GUARANTEE ("commit or rollback"):
 *    - If an operation fails, state is rolled back to before the call.
 *    - Achieved by: copy-and-swap idiom, scope guards.
 *
 * 3. NO-THROW GUARANTEE:
 *    - The operation NEVER throws. Mark with noexcept.
 *    - Required for: destructors, move constructors (for vector safety),
 *      swap operations.
 *
 * In production: aim for STRONG on public APIs, BASIC internally, NO-THROW on
 * move/swap/destructors.
 */

class TransactionalVector {
public:
    // Strong guarantee: either all elements are added, or none are
    void add_batch(const std::vector<int>& items) {
        // Copy current state
        auto backup = data_;  // if this throws, nothing changed

        // Modify the copy
        for (int item : items) {
            if (item < 0) throw std::invalid_argument("negative value");
            backup.push_back(item);
        }

        // Commit (swap is noexcept)
        data_.swap(backup);
    }

    void print() const {
        std::cout << "  Data: ";
        for (int x : data_) std::cout << x << " ";
        std::cout << "\n";
    }

private:
    std::vector<int> data_;
};

void demo_exception_safety() {
    std::cout << "\n=== Exception Safety Demo ===\n";

    TransactionalVector tv;
    tv.add_batch({1, 2, 3});
    tv.print();  // 1 2 3

    try {
        tv.add_batch({4, 5, -1, 6});  // fails at -1
    } catch (const std::exception& e) {
        std::cout << "  Exception: " << e.what() << "\n";
    }
    tv.print();  // still 1 2 3 — strong guarantee!
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== RAII: Mutex ===\n";
    ThreadSafeCounter counter;
    counter.increment();
    counter.increment();
    counter.add(5);
    std::cout << "  Counter: " << counter.get() << "\n";

    std::cout << "\n=== RAII: MappedFile ===\n";
    {
        MappedFile f1("data.bin");
        MappedFile f2 = std::move(f1);  // move, don't copy
        std::cout << "  f2 size: " << f2.size() << "\n";
    }  // f2 destroyed here → cleanup

    demo_scope_guard();
    demo_scope_timer();
    demo_exception_safety();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. RAII = constructor acquires, destructor releases. Period.
// 2. If a class owns any resource, implement the Rule of Five (or delete copy ops).
// 3. Use lock_guard/unique_lock for mutexes, unique_ptr for heap, fstream for files.
// 4. ScopeGuard is your escape hatch for C APIs and custom cleanup.
// 5. Exception safety comes FREE with RAII. Without RAII, it's nearly impossible.
// 6. ALWAYS mark destructors and move ops as noexcept.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a RAII `TempFile` class: constructor creates a temp file, destructor
//    deletes it. Support move but not copy. Use it in a function that writes
//    intermediate results and cleans up automatically.
//
// 2. Implement a `ConnectionPool` that uses RAII to return connections.
//    acquire() returns a special RAII handle; when the handle is destroyed,
//    the connection goes back to the pool (not deleted!).
//    Hint: use unique_ptr with a custom deleter.
//
// 3. Write a `ScopeGuard` that uses std::function instead of a template.
//    Compare the code complexity and discuss the performance tradeoff.
//
// 4. Add a `try_add_batch` to TransactionalVector that returns bool instead
//    of throwing. Which gives a better API? When would you choose each?
// ============================================================================
