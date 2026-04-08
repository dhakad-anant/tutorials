/**
 * Module 01 — Lesson 6: C++20 Preview — Ranges & Concepts
 *
 * WHY THIS MATTERS:
 *   C++20 is the biggest update since C++11. Concepts and Ranges will define how
 *   modern C++ code looks for the next decade. New codebases at Pure Storage will
 *   increasingly adopt these.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o cpp20_preview 06_cpp20_preview.cpp
 */

#include <iostream>
#include <vector>
#include <ranges>
#include <concepts>
#include <string>
#include <algorithm>
#include <numeric>
#include <span>

// ============================================================================
// Part 1: Concepts — Constraining Templates
// ============================================================================

// BEFORE C++20: Template errors were cryptic pages of nonsense.
// "no matching function for call to sort..." with 200 lines of template backtraces.
//
// WITH CONCEPTS: Clear constraints, clear error messages.

// Using standard library concepts
template <std::integral T>
T gcd(T a, T b) {
    while (b != 0) {
        T tmp = b;
        b = a % b;
        a = tmp;
    }
    return a;
}
// gcd(3.14, 2.0)  → compile error: "double does not satisfy integral"

// Defining custom concepts
template <typename T>
concept Serializable = requires(T t, std::ostream& os) {
    { os << t } -> std::same_as<std::ostream&>;  // must support <<
    { t.size() } -> std::convertible_to<size_t>;  // must have .size()
};

template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<size_t>;
};

// Using concepts: 3 equivalent syntaxes
// 1. Requires clause
template <typename T>
    requires std::integral<T>
T abs_val_v1(T x) { return x < 0 ? -x : x; }

// 2. Trailing requires
template <typename T>
T abs_val_v2(T x) requires std::integral<T> { return x < 0 ? -x : x; }

// 3. Constrained auto (shortest)
auto abs_val_v3(std::integral auto x) { return x < 0 ? -x : x; }

// Practical concept: something you can read bytes from
template <typename T>
concept ByteSource = requires(T source, char* buf, size_t n) {
    { source.read(buf, n) } -> std::convertible_to<size_t>;
    { source.eof() } -> std::same_as<bool>;
};

// ============================================================================
// Part 2: Ranges — Composable, Lazy Pipelines
// ============================================================================

void demo_ranges() {
    std::cout << "=== Ranges ===\n";

    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    // Pipeline: filter even → square → take first 4
    // This is LAZY — no intermediate vectors are created!
    auto result = data
        | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([](int x) { return x * x; })
        | std::views::take(4);

    std::cout << "  Even squares (first 4): ";
    for (int x : result) std::cout << x << " ";  // 4 16 36 64
    std::cout << "\n";

    // Reverse view (no copy!)
    std::cout << "  Reversed: ";
    for (int x : data | std::views::reverse | std::views::take(5)) {
        std::cout << x << " ";
    }
    std::cout << "\n";

    // Enumerate-like: iota generates 0, 1, 2, ...
    std::cout << "  Iota [5, 10): ";
    for (int x : std::views::iota(5, 10)) std::cout << x << " ";
    std::cout << "\n";

    // Ranges algorithms (take the range directly, no begin/end needed)
    std::vector<int> nums = {3, 1, 4, 1, 5, 9, 2, 6};
    std::ranges::sort(nums);
    std::cout << "  Sorted: ";
    for (int x : nums) std::cout << x << " ";
    std::cout << "\n";

    // Find with ranges
    auto it = std::ranges::find_if(nums, [](int x) { return x > 4; });
    if (it != nums.end()) {
        std::cout << "  First > 4: " << *it << "\n";
    }
}

// ============================================================================
// Part 3: std::span — Non-owning view of contiguous memory
// ============================================================================

// Like string_view but for any contiguous data

void process_data(std::span<const int> data) {
    std::cout << "  Processing " << data.size() << " elements: ";
    for (int x : data) std::cout << x << " ";
    std::cout << "\n";
}

void demo_span() {
    std::cout << "\n=== std::span ===\n";

    // Works with vector
    std::vector<int> vec = {1, 2, 3, 4, 5};
    process_data(vec);

    // Works with arrays
    int arr[] = {10, 20, 30};
    process_data(arr);

    // Works with subranges
    process_data(std::span(vec).subspan(1, 3));  // {2, 3, 4}

    // Mutable span
    auto modify = [](std::span<int> data) {
        for (auto& x : data) x *= 2;
    };
    modify(vec);
    std::cout << "  After doubling: ";
    for (int x : vec) std::cout << x << " ";
    std::cout << "\n";
}

// ============================================================================
// Part 4: Three-Way Comparison (Spaceship Operator) <=>
// ============================================================================

struct Version {
    int major;
    int minor;
    int patch;

    // One operator replaces ==, !=, <, >, <=, >=
    auto operator<=>(const Version&) const = default;
};

void demo_spaceship() {
    std::cout << "\n=== Spaceship Operator ===\n";

    Version v1{2, 1, 0};
    Version v2{2, 1, 3};
    Version v3{2, 1, 0};

    std::cout << "  v1 < v2: "  << std::boolalpha << (v1 < v2) << "\n";   // true
    std::cout << "  v1 == v3: " << (v1 == v3) << "\n";                      // true
    std::cout << "  v2 >= v1: " << (v2 >= v1) << "\n";                      // true

    // Can even sort a vector of Versions with no custom comparator!
    std::vector<Version> versions = {{1, 0, 0}, {2, 1, 3}, {1, 5, 2}, {2, 0, 0}};
    std::ranges::sort(versions);
    std::cout << "  Sorted versions: ";
    for (const auto& v : versions) {
        std::cout << v.major << "." << v.minor << "." << v.patch << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// Part 5: Designated Initializers, [[likely]], [[unlikely]]
// ============================================================================

struct Config {
    int    block_size  = 4096;
    int    queue_depth = 32;
    bool   compression = true;
    bool   encryption  = false;
};

int process_request(int priority) {
    if (priority > 8) [[unlikely]] {
        // Hot path optimization hint to compiler
        std::cout << "  CRITICAL priority!\n";
        return -1;
    }
    if (priority <= 3) [[likely]] {
        // Most requests are low priority
        return 0;
    }
    return 1;
}

void demo_misc_cpp20() {
    std::cout << "\n=== Misc C++20 Features ===\n";

    // Designated initializers (C99-style, now in C++20)
    Config cfg{
        .block_size = 8192,
        .queue_depth = 64,
        .compression = true,
        .encryption = true
    };
    std::cout << "  Block size: " << cfg.block_size << "\n";
    std::cout << "  Queue depth: " << cfg.queue_depth << "\n";

    process_request(2);
    process_request(9);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Concepts Demo ===\n";
    std::cout << "  gcd(12, 8) = " << gcd(12, 8) << "\n";
    std::cout << "  abs_val(-7) = " << abs_val_v3(-7) << "\n";

    demo_ranges();
    demo_span();
    demo_spaceship();
    demo_misc_cpp20();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Concepts = constraints on template parameters. Use them. Always.
// 2. Ranges = lazy, composable pipelines. No more begin/end pairs.
// 3. span<T> = non-owning view of contiguous data (generalized string_view)
// 4. <=> spaceship operator = write one line, get all 6 comparison operators
// 5. These are the features that define "modern" C++ going forward
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Define a concept `Container` that requires begin(), end(), size(), and empty().
//    Write a function that prints any Container.
//
// 2. Use ranges to: given a vector of strings, filter those starting with "err_",
//    transform to uppercase, and collect into a new vector.
//
// 3. Write a `Matrix` struct with spaceship operator that compares by dimensions
//    first, then element-wise.
//
// 4. Create a concept `StorageDevice` that requires read(), write(), flush(), and
//    capacity(). Write a mock class that satisfies it and a function constrained
//    to only accept StorageDevice types.
// ============================================================================
