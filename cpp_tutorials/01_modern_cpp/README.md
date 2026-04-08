# Module 01: Modern C++ (C++11 / 14 / 17 / 20)

You already know classes, STL containers, and basic templates. This module covers the
**modern features** that changed how production C++ is written.

## Topics
1. [Move Semantics & Rvalue References](01_move_semantics.cpp)
2. [Smart Pointers Deep Dive](02_smart_pointers.cpp)
3. [Lambdas & `std::function`](03_lambdas.cpp)
4. [`constexpr` & Compile-Time Computation](04_constexpr.cpp)
5. [C++17: `optional`, `variant`, `string_view`, structured bindings](05_cpp17_gems.cpp)
6. [C++20: Ranges, Concepts preview](06_cpp20_preview.cpp)

## Key Mental Model Shift
In competitive programming, you optimize for **algorithm correctness and speed of writing**.
In production C++, you optimize for:
- **Ownership clarity** — who owns this memory? Who frees it?
- **Move instead of copy** — avoid unnecessary allocations
- **Express intent** — `const`, `noexcept`, `[[nodiscard]]` tell the reader AND compiler what you mean

## Compile & Run
```bash
g++ -std=c++20 -Wall -Wextra -Werror -o move_semantics 01_move_semantics.cpp && ./move_semantics
```

---
*After completing all files, move to [Module 02: Memory Management](../02_memory_management/README.md) →*
