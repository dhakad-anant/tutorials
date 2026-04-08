/**
 * Module 06 — Lesson 2: Testing, Sanitizers & Build Systems
 *
 * This file teaches testing CONCEPTS and demonstrates test patterns.
 * It also covers the tools that make production C++ reliable.
 *
 * For actual GoogleTest usage, you'll need to install GTest.
 * This file compiles standalone and shows the patterns.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o testing 02_testing_cmake.cpp
 */

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <chrono>
#include <stdexcept>
#include <source_location>

// ============================================================================
// Part 1: Minimal Test Framework (understand the mechanics)
// ============================================================================

class TestFramework {
public:
    static TestFramework& instance() {
        static TestFramework tf;
        return tf;
    }

    void add_test(std::string name, std::function<void()> fn) {
        tests_.push_back({std::move(name), std::move(fn)});
    }

    int run_all() {
        int passed = 0, failed = 0;
        for (const auto& [name, fn] : tests_) {
            try {
                fn();
                ++passed;
                std::cout << "  ✓ " << name << "\n";
            } catch (const std::exception& e) {
                ++failed;
                std::cout << "  ✗ " << name << ": " << e.what() << "\n";
            }
        }
        std::cout << "\n  " << passed << " passed, " << failed << " failed\n";
        return failed;
    }

private:
    std::vector<std::pair<std::string, std::function<void()>>> tests_;
};

// Macros to register tests (similar to TEST() in GTest)
#define TEST_CASE(name)                                              \
    void test_##name();                                              \
    struct TestReg_##name {                                          \
        TestReg_##name() {                                           \
            TestFramework::instance().add_test(#name, test_##name);  \
        }                                                            \
    } testReg_##name;                                                \
    void test_##name()

#define EXPECT_EQ(a, b) do {                                             \
    if ((a) != (b)) {                                                    \
        std::ostringstream oss;                                          \
        oss << "EXPECT_EQ failed: " << (a) << " != " << (b)             \
            << " at " << __FILE__ << ":" << __LINE__;                    \
        throw std::runtime_error(oss.str());                             \
    }                                                                    \
} while(0)

#define EXPECT_TRUE(cond) do {                                           \
    if (!(cond)) {                                                       \
        throw std::runtime_error(                                        \
            std::string("EXPECT_TRUE failed at ") + __FILE__ + ":" +     \
            std::to_string(__LINE__));                                    \
    }                                                                    \
} while(0)

#define EXPECT_THROW(expr, ExType) do {                                  \
    bool caught = false;                                                 \
    try { expr; } catch (const ExType&) { caught = true; }               \
    if (!caught) throw std::runtime_error(                               \
        "Expected " #ExType " not thrown at " __FILE__ ":" +             \
        std::to_string(__LINE__));                                       \
} while(0)

// ============================================================================
// Part 2: Code Under Test
// ============================================================================

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity), data_(capacity), head_(0), tail_(0), size_(0) {}

    bool push(int value) {
        if (size_ == capacity_) return false;
        data_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        return true;
    }

    int pop() {
        if (size_ == 0) throw std::underflow_error("pop on empty buffer");
        int value = data_[head_];
        head_ = (head_ + 1) % capacity_;
        --size_;
        return value;
    }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] bool full() const { return size_ == capacity_; }

private:
    size_t capacity_;
    std::vector<int> data_;
    size_t head_, tail_, size_;
};

// ============================================================================
// Part 3: Test Cases
// ============================================================================

TEST_CASE(ring_buffer_empty_on_creation) {
    RingBuffer rb(10);
    EXPECT_TRUE(rb.empty());
    EXPECT_EQ(rb.size(), 0u);
}

TEST_CASE(ring_buffer_push_pop) {
    RingBuffer rb(5);
    rb.push(42);
    rb.push(99);
    EXPECT_EQ(rb.size(), 2u);
    EXPECT_EQ(rb.pop(), 42);  // FIFO
    EXPECT_EQ(rb.pop(), 99);
    EXPECT_TRUE(rb.empty());
}

TEST_CASE(ring_buffer_full) {
    RingBuffer rb(3);
    EXPECT_TRUE(rb.push(1));
    EXPECT_TRUE(rb.push(2));
    EXPECT_TRUE(rb.push(3));
    EXPECT_TRUE(rb.full());
    EXPECT_TRUE(!rb.push(4));  // should fail
}

TEST_CASE(ring_buffer_wraparound) {
    RingBuffer rb(3);
    rb.push(1); rb.push(2); rb.push(3);
    rb.pop();  // remove 1
    rb.push(4);  // wraps around
    EXPECT_EQ(rb.pop(), 2);
    EXPECT_EQ(rb.pop(), 3);
    EXPECT_EQ(rb.pop(), 4);
}

TEST_CASE(ring_buffer_pop_empty_throws) {
    RingBuffer rb(5);
    EXPECT_THROW(rb.pop(), std::underflow_error);
}

// ============================================================================
// Part 4: Sanitizers Reference
// ============================================================================

/*
 * SANITIZERS: Compile-time flags that add runtime checks for bugs.
 * Use them during development and CI. They catch bugs that tests miss.
 *
 * 1. AddressSanitizer (ASan) — memory errors
 *    g++ -fsanitize=address -fno-omit-frame-pointer -g ...
 *    Catches: buffer overflow, use-after-free, memory leaks
 *
 * 2. UndefinedBehaviorSanitizer (UBSan) — undefined behavior
 *    g++ -fsanitize=undefined -g ...
 *    Catches: signed overflow, null dereference, shift by >= type width
 *
 * 3. ThreadSanitizer (TSan) — data races
 *    g++ -fsanitize=thread -g ...
 *    Catches: data races between threads
 *
 * 4. MemorySanitizer (MSan) — uninitialized reads (Clang only)
 *    clang++ -fsanitize=memory -g ...
 *    Catches: use of uninitialized memory
 *
 * TYPICAL CI PIPELINE:
 *    Build 1: -O2 -Wall -Werror (catch warnings)
 *    Build 2: -O0 -g -fsanitize=address,undefined (catch memory bugs)
 *    Build 3: -O0 -g -fsanitize=thread (catch race conditions)
 */

// ============================================================================
// Part 5: CMake Build System Reference
// ============================================================================

/*
 * MINIMUM VIABLE CMakeLists.txt:
 *
 * cmake_minimum_required(VERSION 3.20)
 * project(storage_engine CXX)
 *
 * set(CMAKE_CXX_STANDARD 20)
 * set(CMAKE_CXX_STANDARD_REQUIRED ON)
 *
 * # Warnings as errors
 * add_compile_options(-Wall -Wextra -Werror -Wpedantic)
 *
 * # Main library
 * add_library(storage_lib
 *     src/block_store.cpp
 *     src/cache.cpp
 *     src/io_manager.cpp
 * )
 *
 * # Main executable
 * add_executable(storage_engine src/main.cpp)
 * target_link_libraries(storage_engine PRIVATE storage_lib)
 *
 * # Tests (using GoogleTest)
 * include(FetchContent)
 * FetchContent_Declare(googletest
 *     GIT_REPOSITORY https://github.com/google/googletest.git
 *     GIT_TAG release-1.12.1
 * )
 * FetchContent_MakeAvailable(googletest)
 *
 * enable_testing()
 * add_executable(storage_tests
 *     tests/block_store_test.cpp
 *     tests/cache_test.cpp
 * )
 * target_link_libraries(storage_tests PRIVATE storage_lib gtest_main)
 * add_test(NAME AllTests COMMAND storage_tests)
 *
 * # Sanitizer build type
 * if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
 *     add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
 *     add_link_options(-fsanitize=address,undefined)
 * endif()
 *
 *
 * BUILD COMMANDS:
 *    mkdir build && cd build
 *    cmake .. -DCMAKE_BUILD_TYPE=Debug
 *    cmake --build .
 *    ctest --output-on-failure
 *
 *    # With sanitizers:
 *    cmake .. -DCMAKE_BUILD_TYPE=ASAN
 */

// ============================================================================
// Part 6: Code Review Checklist
// ============================================================================

/*
 * PRODUCTION C++ CODE REVIEW CHECKLIST:
 *
 * □ OWNERSHIP: Every heap allocation has a clear owner (unique_ptr/shared_ptr)
 * □ RAII: Every resource (file, socket, lock) is managed by RAII
 * □ CONST: Is everything that can be const, const? (params, methods, returns)
 * □ NOEXCEPT: Are move ops and destructors noexcept?
 * □ NODISCARD: Do functions whose return values matter have [[nodiscard]]?
 * □ THREAD SAFETY: Is every shared mutable state protected?
 * □ ERROR HANDLING: Are all errors handled? No silent failures?
 * □ BOUNDS: Are all array/vector accesses bounds-checked in debug?
 * □ OVERFLOW: Can any size_t or int computation overflow?
 * □ LIFETIME: Do all references/pointers outlive their referent?
 * □ PERFORMANCE: Any unnecessary copies? Should something be moved?
 * □ TESTS: Are there unit tests for the new code?
 * □ NAMING: Are names clear? No abbreviations without context?
 */

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Running Tests ===\n";
    int failures = TestFramework::instance().run_all();

    std::cout << "\n=== Additional Notes ===\n";
    std::cout << "  Compile with sanitizers:\n";
    std::cout << "    g++ -std=c++20 -fsanitize=address,undefined -g -o test file.cpp\n";
    std::cout << "  For production, use GoogleTest + CMake + CI pipeline.\n";

    return failures;
}

// ============================================================================
// EXERCISES:
//
// 1. Set up a real CMake project with GoogleTest. Write tests for the RingBuffer
//    using TEST_F (test fixtures) and parameterized tests.
//
// 2. Intentionally write code with:
//    a) A buffer overflow → catch with ASan
//    b) A data race → catch with TSan
//    c) Signed integer overflow → catch with UBSan
//
// 3. Add code coverage: use --coverage flag and lcov/gcov to measure
//    test coverage of RingBuffer. Aim for 100% branch coverage.
//
// 4. Write a benchmark for RingBuffer using Google Benchmark:
//    BM_PushPop, BM_PushFull, BM_PopEmpty.
// ============================================================================
