/**
 * Module 08 — Lesson 1: Performance — Profiling, Benchmarking & Optimization
 *
 * WHY THIS MATTERS:
 *   C++ is chosen for performance-critical code. But writing fast C++ requires
 *   understanding the hardware: CPU caches, branch prediction, instruction-level
 *   parallelism. This module teaches you to measure and optimize systematically.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -O2 -o performance 01_performance.cpp
 *   (For profiling: add -g -fno-omit-frame-pointer)
 */

#include <iostream>
#include <vector>
#include <list>
#include <array>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <string>
#include <cstring>
#include <map>
#include <unordered_map>

// ============================================================================
// Benchmark Utility
// ============================================================================

class Benchmark {
public:
    explicit Benchmark(std::string name) : name_(std::move(name)) {}

    template <typename Func>
    void run(Func&& f, int iterations = 1) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            f();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        double ns_per_op = static_cast<double>(ns.count()) / iterations;
        std::cout << "  " << name_ << ": "
                  << ns_per_op / 1000.0 << " µs/op"
                  << " (" << iterations << " iters)\n";
    }

private:
    std::string name_;
};

// Prevent compiler from optimizing away a value
template <typename T>
void do_not_optimize(T&& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// ============================================================================
// Part 1: Cache-Friendly vs Cache-Hostile Access  
// ============================================================================

void demo_cache_access() {
    std::cout << "=== Cache Access Patterns ===\n";

    constexpr int N = 1024 * 1024;  // ~4MB of ints
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);

    // Sequential access (cache-friendly: one cache line loads 16 ints)
    Benchmark("Sequential").run([&]() {
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += data[i];
        do_not_optimize(sum);
    }, 100);

    // Stride-16 access (one int per cache line — wastes 15/16 of bandwidth)
    Benchmark("Stride-16 ").run([&]() {
        long long sum = 0;
        for (int i = 0; i < N; i += 16) sum += data[i];
        do_not_optimize(sum);
    }, 100);

    // Random access (cache-hostile: every access misses)
    std::vector<int> indices(N);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);

    Benchmark("Random     ").run([&]() {
        long long sum = 0;
        for (int i = 0; i < N; ++i) sum += data[indices[i]];
        do_not_optimize(sum);
    }, 10);
}

// ============================================================================
// Part 2: Container Choice Matters
// ============================================================================

void demo_container_perf() {
    std::cout << "\n=== Container Performance ===\n";

    constexpr int N = 100'000;

    // vector: contiguous, cache-friendly
    Benchmark("vector push_back   ").run([&]() {
        std::vector<int> v;
        for (int i = 0; i < N; ++i) v.push_back(i);
        do_not_optimize(v.back());
    }, 100);

    // list: fragmented, cache-hostile
    Benchmark("list push_back     ").run([&]() {
        std::list<int> l;
        for (int i = 0; i < N; ++i) l.push_back(i);
        do_not_optimize(l.back());
    }, 100);

    // Iteration
    std::vector<int> vec(N);
    std::list<int> lst(N);
    std::iota(vec.begin(), vec.end(), 0);
    std::copy(vec.begin(), vec.end(), lst.begin());

    Benchmark("vector iteration   ").run([&]() {
        long long sum = 0;
        for (int x : vec) sum += x;
        do_not_optimize(sum);
    }, 1000);

    Benchmark("list iteration     ").run([&]() {
        long long sum = 0;
        for (int x : lst) sum += x;
        do_not_optimize(sum);
    }, 1000);

    // Lookup: map vs unordered_map
    std::map<int, int> ordered;
    std::unordered_map<int, int> hashed;
    for (int i = 0; i < N; ++i) {
        ordered[i] = i;
        hashed[i] = i;
    }

    Benchmark("map lookup         ").run([&]() {
        int sum = 0;
        for (int i = 0; i < 1000; ++i) sum += ordered[i];
        do_not_optimize(sum);
    }, 1000);

    Benchmark("unordered_map lookup").run([&]() {
        int sum = 0;
        for (int i = 0; i < 1000; ++i) sum += hashed[i];
        do_not_optimize(sum);
    }, 1000);
}

// ============================================================================
// Part 3: Branch Prediction & Sorted Data
// ============================================================================

void demo_branch_prediction() {
    std::cout << "\n=== Branch Prediction ===\n";

    constexpr int N = 1'000'000;
    std::vector<int> data(N);
    std::mt19937 rng(42);
    for (auto& x : data) x = rng() % 256;

    // Unsorted: branch predictor can't predict the if condition
    Benchmark("Unsorted conditional").run([&]() {
        long long sum = 0;
        for (int x : data) {
            if (x >= 128) sum += x;  // unpredictable branch
        }
        do_not_optimize(sum);
    }, 100);

    // Sorted: branch predictor nails it (first half < 128, second half >= 128)
    std::sort(data.begin(), data.end());

    Benchmark("Sorted conditional  ").run([&]() {
        long long sum = 0;
        for (int x : data) {
            if (x >= 128) sum += x;  // very predictable
        }
        do_not_optimize(sum);
    }, 100);

    // Branchless: avoid the branch entirely
    Benchmark("Branchless          ").run([&]() {
        long long sum = 0;
        for (int x : data) {
            sum += (x >= 128) * x;  // no branch! multiply by 0 or 1
        }
        do_not_optimize(sum);
    }, 100);
}

// ============================================================================
// Part 4: String Optimization
// ============================================================================

void demo_string_perf() {
    std::cout << "\n=== String Performance ===\n";

    // Small strings: SBO avoids heap allocation
    Benchmark("SBO string creation ").run([]() {
        std::string s = "hello";  // fits in SBO (< ~22 chars)
        do_not_optimize(s);
    }, 1'000'000);

    // Large strings: heap allocation
    Benchmark("Heap string creation").run([]() {
        std::string s(100, 'x');  // heap-allocated
        do_not_optimize(s);
    }, 1'000'000);

    // String concatenation: reserve vs naive
    Benchmark("Naive concat        ").run([]() {
        std::string s;
        for (int i = 0; i < 1000; ++i) s += "x";
        do_not_optimize(s);
    }, 1000);

    Benchmark("Reserved concat     ").run([]() {
        std::string s;
        s.reserve(1000);
        for (int i = 0; i < 1000; ++i) s += "x";
        do_not_optimize(s);
    }, 1000);

    // string_view: zero-copy substring
    std::string base(10000, 'A');
    Benchmark("substr (copy)       ").run([&]() {
        auto s = base.substr(100, 500);
        do_not_optimize(s);
    }, 100'000);

    Benchmark("string_view (nocopy)").run([&]() {
        std::string_view sv(base);
        auto s = sv.substr(100, 500);
        do_not_optimize(s);
    }, 100'000);
}

// ============================================================================
// Part 5: Optimization Checklist
// ============================================================================

/*
 * PROFILING TOOLS:
 *   - perf stat ./program                    — hardware counters (cache misses, branches)
 *   - perf record ./program && perf report   — hotspot profiling
 *   - valgrind --tool=cachegrind ./program    — cache simulation
 *   - Intel VTune                              — deep CPU analysis
 *   - Google Benchmark                         — microbenchmarking framework
 *   - Tracy / Optick                           — real-time frame profilers
 *
 * COMPILER FLAGS:
 *   -O2          — standard optimization (default for production)
 *   -O3          — more aggressive (may increase code size)
 *   -march=native — optimize for local CPU (SSE, AVX, etc.)
 *   -flto        — link-time optimization (cross-file inlining)
 *   -fprofile-generate / -fprofile-use — profile-guided optimization (PGO)
 *
 * OPTIMIZATION CHECKLIST (in order of ROI):
 * 
 * 1. ALGORITHM: O(n) vs O(n²) dwarfs all micro-optimizations. Fix the algorithm first.
 * 
 * 2. DATA STRUCTURE: Use contiguous memory (vector > list > map for iteration).
 *    Flat arrays beat everything for sequential access.
 *
 * 3. MEMORY ACCESS: Cache-friendly patterns. AoS vs SoA. Avoid pointer chasing.
 *    Prefetch hints for known access patterns.
 *
 * 4. ALLOCATION: Minimize heap allocations in hot paths. Use pools, arenas, SBO.
 *    reserve() vectors. Move instead of copy.
 *
 * 5. BRANCHING: Sort data if it enables branch prediction. Use branchless techniques
 *    for simple conditions. Use [[likely]]/[[unlikely]].
 *
 * 6. INLINING: Small hot functions should be in headers (or use LTO).
 *    Mark with inline or let the compiler decide at -O2.
 *
 * 7. SIMD: For data-parallel operations (checksums, compression, search).
 *    Use intrinsics or let the compiler auto-vectorize (-ftree-vectorize).
 *
 * 8. CONCURRENCY: Use parallelism but measure the overhead. Thread creation costs ~50µs.
 *    Lock contention kills parallelism. Prefer lock-free or work-stealing.
 *
 * RULES:
 * - Profile BEFORE optimizing. You are ALWAYS wrong about where the bottleneck is.
 * - Optimize the hot path (inner loops, request handlers). Don't touch cold code.
 * - Readability > cleverness UNTIL profiling proves otherwise.
 */

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_cache_access();
    demo_container_perf();
    demo_branch_prediction();
    demo_string_perf();

    std::cout << "\n=== Summary ===\n";
    std::cout << "  1. Sequential memory access >> random access\n";
    std::cout << "  2. vector >> list for almost everything\n";
    std::cout << "  3. Sorted data helps branch prediction\n";
    std::cout << "  4. string_view avoids copies for substrings\n";
    std::cout << "  5. reserve() prevents reallocation\n";
    std::cout << "  6. ALWAYS measure. NEVER guess.\n";

    return 0;
}

// ============================================================================
// EXERCISES:
//
// 1. Benchmark: binary_search on a sorted vector vs find in an unordered_set.
//    At what size does the hash set start winning? (Test N=10, 100, 10000)
//
// 2. Implement the same algorithm (e.g., matrix multiply) in three ways:
//    naive (i,j,k), cache-friendly (i,k,j), and SIMD-hinted. Benchmark all three.
//
// 3. Profile a program with `perf record` and `perf report`. Identify the
//    hottest function and optimize it. Document the before/after numbers.
//
// 4. Write a microbenchmark suite for your RingBuffer (from Module 06):
//    push throughput, pop throughput, mixed push/pop. Use the Benchmark class.
//
// 5. CAPSTONE PROJECT: Build a simple in-memory key-value store that:
//    - Uses a hash map with custom allocator (pool allocator from Module 02)
//    - Supports concurrent reads/writes (shared_mutex from Module 04)
//    - Serializes to disk in a binary format (from Module 05)
//    - Has unit tests and benchmarks
//    - Handles errors with Result<T,E>
//    This integrates everything from all 8 modules.
// ============================================================================
