# Advanced C++ Tutorial — From Intermediate to Production-Grade

A structured, hands-on tutorial to take you from intermediate C++ (comfortable with DSA)
to writing **production-grade systems code** — the kind expected at companies like Pure Storage.

## Who This Is For
- You can solve DSA problems in C++ confidently
- You know STL containers, iterators, basic OOP
- You want to write **real-world, high-performance, maintainable** C++

## What You'll Learn

| Module | Topic | Key Skills |
|--------|-------|------------|
| 01 | [Modern C++ (11/14/17/20)](01_modern_cpp/) | Move semantics, smart pointers, lambdas, `constexpr`, structured bindings, `std::optional`, `std::variant` |
| 02 | [Memory Management & Ownership](02_memory_management/) | RAII, smart pointers deep dive, custom allocators, memory layout, cache efficiency |
| 03 | [Templates & Metaprogramming](03_templates/) | Variadic templates, SFINAE, `if constexpr`, concepts (C++20), type traits, CRTP |
| 04 | [Concurrency & Multithreading](04_concurrency/) | `std::thread`, mutexes, condition variables, atomics, lock-free programming, thread pools |
| 05 | [Systems Programming Patterns](05_systems_programming/) | File I/O, mmap, serialization, zero-copy, network sockets, IPC |
| 06 | [Production-Grade Practices](06_production/) | Error handling strategies, logging, testing (GTest), CMake, sanitizers, code review checklist |
| 07 | [Design Patterns in C++](07_design_patterns/) | PIMPL, type erasure, visitor, observer, builder — the C++ way |
| 08 | [Performance & Optimization](08_performance/) | Profiling, cache-friendly code, branch prediction, SIMD intro, benchmarking with Google Benchmark |

## How to Use This Tutorial
1. **Go in order** — each module builds on the previous
2. **Type every example** — don't copy-paste; muscle memory matters
3. **Do the exercises** — they simulate real production scenarios
4. **Compile with warnings**: `g++ -std=c++20 -Wall -Wextra -Werror -O2`
5. **Use sanitizers** during development: `-fsanitize=address,undefined`

## Build Requirements
- **Compiler**: GCC 12+ or Clang 15+ or MSVC 2022 (with C++20 support)
- **Build system**: CMake 3.20+ (introduced in Module 06)
- **OS**: Linux recommended; Windows with WSL or MSVC works too

## Pure Storage Context
Pure Storage builds high-performance **storage systems** in C++. The codebase likely involves:
- High-throughput I/O paths (Module 05)
- Lock-free data structures and concurrency (Module 04)
- Custom memory management (Module 02)
- Template-heavy infrastructure code (Module 03)
- Strict code quality: testing, sanitizers, code review (Module 06)

This tutorial is designed with those demands in mind.

---
*Start with [Module 01: Modern C++](01_modern_cpp/README.md) →*
