/**
 * Module 02 — Lesson 2: Memory Layout — Stack, Heap, Static, Cache Lines
 *
 * WHY THIS MATTERS:
 *   Understanding WHERE your data lives determines performance. In storage systems,
 *   the difference between cache-friendly and cache-hostile code can be 100x.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -O2 -o memory_layout 02_memory_layout.cpp
 */

#include <iostream>
#include <vector>
#include <list>
#include <array>
#include <chrono>
#include <random>
#include <memory>
#include <cstdint>
#include <new>     // std::hardware_destructive_interference_size

// ============================================================================
// Part 1: Where Data Lives
// ============================================================================

// STATIC / GLOBAL — lives for entire program lifetime
static int global_counter = 0;
// Initialized before main(), destroyed after main()
// Problem: "static initialization order fiasco" — order between translation units undefined

void demo_memory_regions() {
    std::cout << "=== Memory Regions ===\n";

    // STACK — fast, automatic, limited size (~1-8 MB)
    int stack_var = 42;
    char stack_buffer[256];
    std::array<int, 100> stack_array;

    // HEAP — slower, manual/smart-ptr managed, virtually unlimited
    auto heap_int = std::make_unique<int>(42);
    auto heap_vec = std::make_unique<std::vector<int>>(1000);

    std::cout << "  Stack var addr:    " << &stack_var << "\n";
    std::cout << "  Stack buffer addr: " << static_cast<void*>(stack_buffer) << "\n";
    std::cout << "  Heap int addr:     " << heap_int.get() << "\n";
    std::cout << "  Global addr:       " << &global_counter << "\n";

    // Note: stack addresses are typically high, heap addresses lower (on x86)
    // The exact layout is OS-dependent

    // DANGER: Stack overflow from large local arrays
    // int too_big[10'000'000];  // ~40MB — will crash on most systems
    // Use heap for large allocations:
    auto big = std::make_unique<int[]>(10'000'000);  // fine
}

// ============================================================================
// Part 2: Object Layout and sizeof
// ============================================================================

struct Unpadded {
    char a;     // 1 byte
    char b;     // 1 byte
    char c;     // 1 byte
    char d;     // 1 byte
};  // sizeof = 4

struct Padded {
    char  a;     // 1 byte  + 7 bytes padding (align to 8)
    double b;    // 8 bytes
    char  c;     // 1 byte  + 3 bytes padding (align to 4)
    int   d;     // 4 bytes
};  // sizeof = 24  (could be 14 without padding)

struct Optimized {
    double b;    // 8 bytes
    int    d;    // 4 bytes
    char   a;    // 1 byte
    char   c;    // 1 byte + 2 bytes padding
};  // sizeof = 16  (same data, better layout!)

struct alignas(64) CacheLineAligned {
    int data;
    // Padded to 64 bytes to avoid false sharing in multi-threaded code
};

void demo_layout() {
    std::cout << "\n=== Object Layout ===\n";
    std::cout << "  sizeof(Unpadded):         " << sizeof(Unpadded) << "\n";   // 4
    std::cout << "  sizeof(Padded):           " << sizeof(Padded) << "\n";     // 24
    std::cout << "  sizeof(Optimized):        " << sizeof(Optimized) << "\n";  // 16
    std::cout << "  sizeof(CacheLineAligned): " << sizeof(CacheLineAligned) << "\n"; // 64

    // Lesson: ORDER YOUR STRUCT MEMBERS BY SIZE (largest first) to minimize padding
    // This is a common code review comment in production.

    // alignof tells you the alignment requirement
    std::cout << "  alignof(double):          " << alignof(double) << "\n";   // 8
    std::cout << "  alignof(CacheLineAligned):" << alignof(CacheLineAligned) << "\n"; // 64
}

// ============================================================================
// Part 3: Cache Performance — Array of Structs vs Struct of Arrays
// ============================================================================

// Pattern 1: Array of Structs (AoS) — natural, but can be cache-hostile
struct ParticleAoS {
    float x, y, z;      // position (used in physics update)
    float r, g, b;      // color (used only in rendering)
    float mass;          // used in physics
    int   id;            // used in lookups
};
// When iterating to update physics, we load x,y,z,mass but also pull in r,g,b,id
// into cache lines — wasting ~50% of cache bandwidth!

// Pattern 2: Struct of Arrays (SoA) — cache-friendly for specific access patterns
struct ParticlesSoA {
    std::vector<float> x, y, z;       // positions contiguous in memory
    std::vector<float> r, g, b;       // colors contiguous (separate from positions)
    std::vector<float> mass;
    std::vector<int>   id;
};

void demo_cache_performance() {
    std::cout << "\n=== Cache Performance: vector vs list ===\n";

    constexpr int N = 1'000'000;

    // Vector: contiguous memory → cache-FRIENDLY
    std::vector<int> vec(N);
    std::iota(vec.begin(), vec.end(), 0);

    // List: scattered heap allocations → cache-HOSTILE
    std::list<int> lst(vec.begin(), vec.end());

    // Benchmark: sum all elements
    auto bench = [](const auto& container, const char* name) {
        auto start = std::chrono::high_resolution_clock::now();
        long long sum = 0;
        for (const auto& x : container) sum += x;
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  " << name << ": sum=" << sum << " in " << us.count() << " µs\n";
    };

    bench(vec, "vector (contiguous)");
    bench(lst, "list   (scattered) ");
    // vector is typically 5-20x faster for sequential access!

    std::cout << "\n  Why? Each cache line is 64 bytes. Vector loads 16 ints per line.\n";
    std::cout << "  List loads 1 int per line (node = int + 2 pointers = ~24 bytes).\n";
}

// ============================================================================
// Part 4: Small Buffer Optimization (SBO)
// ============================================================================

/*
 * std::string typically uses SBO: short strings (≤15-22 chars depending on
 * implementation) are stored INSIDE the string object itself (on the stack),
 * not on the heap. This avoids a heap allocation for the majority of strings.
 *
 * Same idea: std::function typically uses SBO for small callables.
 *
 * This is why sizeof(std::string) is 32 bytes, not 8.
 */

void demo_sbo() {
    std::cout << "\n=== Small Buffer Optimization ===\n";
    std::cout << "  sizeof(std::string) = " << sizeof(std::string) << "\n";

    std::string short_str = "hi";          // stored in-place (SBO)
    std::string long_str(100, 'x');        // heap-allocated

    // You can observe this: short_str's data is near &short_str
    std::cout << "  &short_str  = " << &short_str << "\n";
    std::cout << "  short data  = " << static_cast<const void*>(short_str.data()) << "\n";
    std::cout << "  &long_str   = " << &long_str << "\n";
    std::cout << "  long data   = " << static_cast<const void*>(long_str.data()) << "\n";
    // short_str.data() is close to &short_str (within the object)
    // long_str.data() is far away (heap address)
}

// ============================================================================
// Part 5: Placement New — Constructing Objects in Pre-Allocated Memory
// ============================================================================

void demo_placement_new() {
    std::cout << "\n=== Placement New ===\n";

    // Allocate raw memory (no constructor called)
    alignas(std::string) char buffer[sizeof(std::string)];

    // Construct a string IN the pre-allocated buffer
    std::string* s = new (buffer) std::string("hello from placement new!");
    std::cout << "  " << *s << "\n";

    // MUST manually call destructor (no delete — we don't own the memory via new)
    s->~basic_string();

    // This pattern is used inside:
    // - std::vector (reserves raw memory, constructs elements in-place)
    // - Memory pools and arena allocators
    // - Serialization (construct from network buffer)
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_memory_regions();
    demo_layout();
    demo_cache_performance();
    demo_sbo();
    demo_placement_new();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Stack = fast, small, automatic. Heap = slower, large, manual/smart-ptr.
// 2. ORDER STRUCT MEMBERS BY SIZE to minimize padding. Use sizeof/alignof to check.
// 3. Contiguous memory (vector, array) is 5-20x faster than scattered (list, map)
//    for sequential access because of CPU cache lines (64 bytes).
// 4. AoS vs SoA: choose based on access pattern. SoA wins when you only touch a
//    subset of fields in a hot loop.
// 5. SBO: small strings/functors avoid heap allocation. Know when it kicks in.
// 6. Placement new: for memory pools and custom allocators. Always call destructor.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a struct with 5 mixed-type fields. Rearrange members to minimize
//    sizeof. Verify with sizeof(). What's the theoretical minimum?
//
// 2. Benchmark: iterate a vector<int> vs deque<int> vs list<int> of 10M elements.
//    Measure sum time. Explain the results in terms of cache behavior.
//
// 3. Implement a simple SoA particle system. Benchmark updating positions
//    (x += vx * dt) for AoS vs SoA with 1M particles.
//
// 4. Write a StackAllocator that carves allocations from a fixed char[4096] buffer
//    using placement new. It should support allocate<T>() and reset().
// ============================================================================
