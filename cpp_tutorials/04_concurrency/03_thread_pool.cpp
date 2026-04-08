/**
 * Module 04 — Lesson 3: Thread Pool & Async Patterns
 *
 * WHY THIS MATTERS:
 *   Creating a thread per task is expensive. Production systems use thread pools
 *   to reuse threads, control concurrency, and manage work queues. This is how
 *   I/O dispatchers and request handlers work at Pure Storage.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -pthread -o thread_pool 03_thread_pool.cpp
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <vector>
#include <future>
#include <chrono>
#include <string>

// ============================================================================
// Part 1: Simple Thread Pool
// ============================================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

                        if (stop_ && tasks_.empty()) return;

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();  // execute outside the lock
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            w.join();
        }
    }

    // Submit a task and get a future for the result
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> future = task->get_future();

        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("submit on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    // Fire-and-forget task
    void enqueue(std::function<void()> task) {
        {
            std::lock_guard lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    // Delete copy
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

void demo_thread_pool() {
    std::cout << "=== Thread Pool ===\n";

    ThreadPool pool(4);

    // Submit tasks with futures
    std::vector<std::future<int>> results;
    for (int i = 0; i < 8; ++i) {
        results.push_back(pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return i * i;
        }));
    }

    // Collect results
    std::cout << "  Results: ";
    for (auto& f : results) {
        std::cout << f.get() << " ";  // blocks until result is ready
    }
    std::cout << "\n";
}

// ============================================================================
// Part 2: std::async & std::future
// ============================================================================

int compute_checksum(const std::string& data) {
    int sum = 0;
    for (char c : data) sum += c;
    return sum;
}

void demo_async() {
    std::cout << "\n=== std::async & std::future ===\n";

    // Launch async tasks
    auto f1 = std::async(std::launch::async, compute_checksum, "block_A_data");
    auto f2 = std::async(std::launch::async, compute_checksum, "block_B_data");
    auto f3 = std::async(std::launch::async, compute_checksum, "block_C_data");

    // Results are computed in parallel
    std::cout << "  Checksum A: " << f1.get() << "\n";
    std::cout << "  Checksum B: " << f2.get() << "\n";
    std::cout << "  Checksum C: " << f3.get() << "\n";

    // std::async with deferred execution (lazy — runs when you call .get())
    auto lazy = std::async(std::launch::deferred, []() {
        std::cout << "  Lazy task executed!\n";
        return 42;
    });
    std::cout << "  Lazy not yet executed...\n";
    std::cout << "  Lazy result: " << lazy.get() << "\n";  // NOW it runs
}

// ============================================================================
// Part 3: Promise/Future — Manual Value Communication
// ============================================================================

void demo_promise() {
    std::cout << "\n=== std::promise & std::future ===\n";

    std::promise<std::string> promise;
    std::future<std::string> future = promise.get_future();

    // Worker thread sets the value
    std::jthread worker([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        promise.set_value("I/O completed successfully");
    });

    // Main thread waits for the value
    std::cout << "  Waiting for result...\n";
    std::string result = future.get();  // blocks until promise is fulfilled
    std::cout << "  Got: " << result << "\n";

    // Promise can also set exceptions:
    std::promise<int> err_promise;
    auto err_future = err_promise.get_future();
    err_promise.set_exception(std::make_exception_ptr(
        std::runtime_error("disk failure")));
    try {
        err_future.get();
    } catch (const std::exception& e) {
        std::cout << "  Exception from future: " << e.what() << "\n";
    }
}

// ============================================================================
// Part 4: Parallel Map/Reduce with Thread Pool
// ============================================================================

void demo_parallel_reduce() {
    std::cout << "\n=== Parallel Map-Reduce ===\n";

    ThreadPool pool(4);
    std::vector<int> data(1'000'000);
    std::iota(data.begin(), data.end(), 1);  // 1, 2, ..., 1M

    // Split into chunks, sum each in parallel
    constexpr int CHUNKS = 8;
    size_t chunk_size = data.size() / CHUNKS;
    std::vector<std::future<long long>> futures;

    for (int i = 0; i < CHUNKS; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == CHUNKS - 1) ? data.size() : start + chunk_size;
        futures.push_back(pool.submit([&data, start, end]() {
            long long sum = 0;
            for (size_t j = start; j < end; ++j) {
                sum += data[j];
            }
            return sum;
        }));
    }

    // Reduce: sum the partial sums
    long long total = 0;
    for (auto& f : futures) {
        total += f.get();
    }
    long long expected = static_cast<long long>(data.size()) * (data.size() + 1) / 2;
    std::cout << "  Sum of 1..1M = " << total << " (expected " << expected << ")\n";
}

// ============================================================================
// Part 5: Common Async Patterns Summary
// ============================================================================

/*
 * PATTERN 1: Fire-and-Forget
 *   pool.enqueue(task);
 *   // Don't care about result
 *
 * PATTERN 2: Submit-and-Collect
 *   auto future = pool.submit(task);
 *   auto result = future.get();  // block until done
 *
 * PATTERN 3: Fan-Out / Fan-In (Map-Reduce)
 *   Submit N tasks, collect all N futures, combine results
 *
 * PATTERN 4: Pipeline
 *   Stage 1 → queue → Stage 2 → queue → Stage 3
 *   Each stage has its own thread pool
 *
 * PATTERN 5: Event Loop (single-threaded async)
 *   One thread processes a queue of callbacks/coroutines
 *   (C++20 coroutines make this elegant — advanced topic)
 *
 * WHEN TO USE WHAT:
 *   - Thread pool: long-lived, bounded concurrency
 *   - std::async: quick parallelism, fire-and-forget
 *   - std::jthread: background worker that runs for the app's lifetime
 */

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_thread_pool();
    demo_async();
    demo_promise();
    demo_parallel_reduce();
    return 0;
}

// ============================================================================
// EXERCISES:
//
// 1. Add a `pending_count()` method to ThreadPool using std::atomic.
//    Track how many tasks are currently pending vs executing.
//
// 2. Implement a PriorityThreadPool where tasks have priorities.
//    Use std::priority_queue instead of std::queue.
//
// 3. Write a parallel merge sort using the thread pool. Split the array,
//    sort halves in parallel, then merge.
//
// 4. Implement a simple "async file reader": submit file paths to the pool,
//    get futures that resolve to file contents (as strings).
// ============================================================================
