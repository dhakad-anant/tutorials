/**
 * Module 01 — Lesson 4: constexpr & Compile-Time Computation
 *
 * WHY THIS MATTERS:
 *   constexpr moves computation from runtime to compile-time. In storage systems,
 *   this is used for compile-time hash tables, buffer size calculations, protocol
 *   constants, and static assertions.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o constexpr_demo 04_constexpr.cpp
 */

#include <iostream>
#include <array>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <numeric>

// ============================================================================
// Part 1: constexpr Functions
// ============================================================================

// A constexpr function CAN be evaluated at compile time (if given constexpr args)
// but also works at runtime with non-constexpr args. Best of both worlds.

constexpr int factorial(int n) {
    // C++14 allows loops and local variables in constexpr functions
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

constexpr int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

// static_assert proves it's computed at compile time
static_assert(factorial(5) == 120, "factorial(5) should be 120");
static_assert(fibonacci(10) == 55, "fib(10) should be 55");

// ============================================================================
// Part 2: constexpr Classes
// ============================================================================

class Vec3 {
public:
    constexpr Vec3(double x, double y, double z) : x_(x), y_(y), z_(z) {}

    constexpr double dot(const Vec3& other) const {
        return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
    }

    constexpr Vec3 operator+(const Vec3& other) const {
        return Vec3(x_ + other.x_, y_ + other.y_, z_ + other.z_);
    }

    constexpr double x() const { return x_; }
    constexpr double y() const { return y_; }
    constexpr double z() const { return z_; }

private:
    double x_, y_, z_;
};

constexpr Vec3 origin{0, 0, 0};
constexpr Vec3 unit_x{1, 0, 0};
constexpr Vec3 unit_y{0, 1, 0};
constexpr Vec3 diagonal = unit_x + unit_y;
static_assert(diagonal.x() == 1.0 && diagonal.y() == 1.0 && diagonal.z() == 0.0);

// ============================================================================
// Part 3: constexpr vs const vs consteval vs constinit
// ============================================================================

/*
 * const      — value cannot change after initialization (but computed at runtime OK)
 * constexpr  — value MUST be computable at compile time (variables) /
 *              CAN be (functions). Also implies const for variables.
 * consteval  — (C++20) function MUST be evaluated at compile time.
 *              Cannot be called at runtime. Immediate function.
 * constinit  — (C++20) variable must be initialized at compile time,
 *              but CAN be modified at runtime. Avoids static init order fiasco.
 */

consteval int must_be_compiletime(int n) {
    return n * n;
}

// This works:
constexpr int ct_result = must_be_compiletime(7);  // 49

// This would NOT compile:
// int x = 5;
// int rt_result = must_be_compiletime(x);  // ERROR: x is not constexpr

// ============================================================================
// Part 4: Compile-Time Lookup Table (Practical Example)
// ============================================================================

// Suppose we need a CRC8 lookup table for a storage protocol
constexpr std::array<uint8_t, 256> generate_crc8_table() {
    std::array<uint8_t, 256> table{};
    for (int i = 0; i < 256; ++i) {
        uint8_t crc = static_cast<uint8_t>(i);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;  // polynomial
            } else {
                crc <<= 1;
            }
        }
        table[i] = crc;
    }
    return table;
}

// Computed ENTIRELY at compile time — zero runtime cost
constexpr auto crc8_table = generate_crc8_table();

constexpr uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

// ============================================================================
// Part 5: if constexpr (C++17) — Compile-Time Branching
// ============================================================================

template <typename T>
std::string type_name() {
    // The "dead" branch is not even compiled.
    // Without `if constexpr`, you'd need template specialization.
    if constexpr (std::is_integral_v<T>) {
        return "integer";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "floating point";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "string";
    } else {
        return "unknown";
    }
}

template <typename T>
void serialize(const T& value) {
    if constexpr (std::is_arithmetic_v<T>) {
        std::cout << "  Serializing number: " << value << "\n";
    } else if constexpr (std::is_same_v<T, std::string>) {
        std::cout << "  Serializing string (len=" << value.size() << "): " << value << "\n";
    } else {
        // This path is only compiled if instantiated with a non-matching type
        static_assert(sizeof(T) == 0, "Unsupported type for serialize");
    }
}

// ============================================================================
// Part 6: constexpr std::array Operations (C++20)
// ============================================================================

constexpr auto make_primes_under_50() {
    // Sieve at compile time!
    std::array<bool, 50> is_prime{};
    for (size_t i = 2; i < 50; ++i) is_prime[i] = true;

    for (size_t i = 2; i * i < 50; ++i) {
        if (is_prime[i]) {
            for (size_t j = i * i; j < 50; j += i) {
                is_prime[j] = false;
            }
        }
    }

    // Count primes
    int count = 0;
    for (size_t i = 2; i < 50; ++i) {
        if (is_prime[i]) ++count;
    }

    // Collect into array
    std::array<int, 15> primes{};  // we know there are 15 primes under 50
    int idx = 0;
    for (size_t i = 2; i < 50 && idx < 15; ++i) {
        if (is_prime[i]) primes[idx++] = static_cast<int>(i);
    }
    return primes;
}

constexpr auto primes_under_50 = make_primes_under_50();
static_assert(primes_under_50[0] == 2);
static_assert(primes_under_50[14] == 47);

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== constexpr Functions ===\n";
    std::cout << "  factorial(5)  = " << factorial(5) << "\n";
    std::cout << "  fibonacci(10) = " << fibonacci(10) << "\n";

    // Also works at runtime
    int n;
    std::cout << "  Enter a number for factorial: ";
    std::cin >> n;
    std::cout << "  factorial(" << n << ") = " << factorial(n) << "\n";

    std::cout << "\n=== constexpr Class ===\n";
    std::cout << "  diagonal = (" << diagonal.x() << ", " << diagonal.y()
              << ", " << diagonal.z() << ")\n";

    std::cout << "\n=== Compile-Time CRC8 Table ===\n";
    std::cout << "  table[0x00] = " << static_cast<int>(crc8_table[0]) << "\n";
    std::cout << "  table[0xFF] = " << static_cast<int>(crc8_table[255]) << "\n";

    std::cout << "\n=== if constexpr ===\n";
    std::cout << "  int    → " << type_name<int>() << "\n";
    std::cout << "  double → " << type_name<double>() << "\n";
    std::cout << "  string → " << type_name<std::string>() << "\n";
    serialize(42);
    serialize(3.14);
    serialize(std::string("hello"));

    std::cout << "\n=== Compile-Time Primes ===\n";
    std::cout << "  Primes under 50: ";
    for (int p : primes_under_50) std::cout << p << " ";
    std::cout << "\n";

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. constexpr functions = "evaluate at compile time IF possible, runtime otherwise"
// 2. consteval = "MUST be compile time" (use for code-gen and config)
// 3. constinit = "compile-time init, but mutable at runtime" (static init safety)
// 4. if constexpr = compile-time branching; dead branches don't compile
// 5. Lookup tables, hash functions, protocol constants → perfect for constexpr
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a constexpr `power(base, exp)` function. Verify with static_assert.
//
// 2. Create a constexpr `RingBuffer<T, N>` that supports push and pop at
//    compile time. Initialize it with values and static_assert the results.
//
// 3. Write a compile-time string hash function (e.g., FNV-1a). Use it to
//    implement a compile-time switch-on-string pattern:
//      switch (hash("status")) { case hash("status"): ... }
//
// 4. Use if constexpr to write a single `to_string(T)` function that handles
//    int, double, bool, and std::string differently.
// ============================================================================
