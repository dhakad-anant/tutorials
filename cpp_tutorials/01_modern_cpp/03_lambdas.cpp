/**
 * Module 01 — Lesson 3: Lambdas & std::function
 *
 * WHY THIS MATTERS:
 *   Lambdas replaced function pointers and functors for callbacks, comparators,
 *   and inline logic. Production codebases use them extensively with STL algorithms,
 *   async operations, and event handling.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o lambdas 03_lambdas.cpp
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <string>
#include <memory>
#include <numeric>

// ============================================================================
// Part 1: Lambda Syntax Progression
// ============================================================================

void demo_lambda_basics() {
    std::cout << "=== Lambda Basics ===\n";

    // Simplest lambda
    auto greet = []() { std::cout << "  Hello, world!\n"; };
    greet();

    // Lambda with parameters
    auto add = [](int a, int b) { return a + b; };
    std::cout << "  3 + 4 = " << add(3, 4) << "\n";

    // Lambda with explicit return type (needed when ambiguous)
    auto divide = [](double a, double b) -> double {
        if (b == 0.0) return 0.0;
        return a / b;
    };
    std::cout << "  10.0 / 3.0 = " << divide(10.0, 3.0) << "\n";
}

// ============================================================================
// Part 2: Captures — The Key Concept
// ============================================================================

void demo_captures() {
    std::cout << "\n=== Captures ===\n";

    int threshold = 50;
    std::string prefix = "Value: ";

    // Capture by value (makes a copy at the point of lambda creation)
    auto by_value = [threshold, prefix](int x) {
        std::cout << "  " << prefix << x << (x > threshold ? " ABOVE" : " below") << "\n";
    };
    threshold = 100;  // Does NOT affect the lambda — it captured 50
    by_value(75);     // "Value: 75 ABOVE" (compares against 50, not 100)

    // Capture by reference
    int count = 0;
    auto by_ref = [&count]() { ++count; };
    by_ref();
    by_ref();
    by_ref();
    std::cout << "  count after 3 calls: " << count << "\n";  // 3

    // DANGER: Capturing by reference to a local that goes out of scope = dangling!
    // This is the #1 lambda bug in production code. More on this below.

    // Capture all by value [=] or all by reference [&] (use sparingly)
    auto capture_all = [=]() {
        std::cout << "  threshold=" << threshold << " prefix=" << prefix << "\n";
    };
    capture_all();

    // C++14: Generalized capture (init capture) — move into lambda
    auto data = std::make_unique<std::vector<int>>(std::vector<int>{1, 2, 3});
    auto moved_capture = [data = std::move(data)]() {
        std::cout << "  Moved vector size: " << data->size() << "\n";
    };
    moved_capture();
    // `data` is now nullptr — it was moved into the lambda
}

// ============================================================================
// Part 3: Lambdas with STL Algorithms
// ============================================================================

void demo_stl_algorithms() {
    std::cout << "\n=== Lambdas with STL ===\n";

    std::vector<int> nums = {5, 2, 8, 1, 9, 3, 7, 4, 6};

    // Sort descending
    std::sort(nums.begin(), nums.end(), [](int a, int b) { return a > b; });
    std::cout << "  Sorted desc: ";
    for (int n : nums) std::cout << n << " ";
    std::cout << "\n";

    // Count elements above threshold
    int threshold = 5;
    auto above = std::count_if(nums.begin(), nums.end(),
                                [threshold](int x) { return x > threshold; });
    std::cout << "  Elements > " << threshold << ": " << above << "\n";

    // Transform: square each element
    std::vector<int> squared(nums.size());
    std::transform(nums.begin(), nums.end(), squared.begin(),
                   [](int x) { return x * x; });
    std::cout << "  Squared: ";
    for (int n : squared) std::cout << n << " ";
    std::cout << "\n";

    // Accumulate with lambda (C++20 ranges make this even better)
    auto sum = std::accumulate(nums.begin(), nums.end(), 0,
                                [](int acc, int x) { return acc + x; });
    std::cout << "  Sum: " << sum << "\n";

    // Partition: odds first, evens last
    std::partition(nums.begin(), nums.end(), [](int x) { return x % 2 != 0; });
    std::cout << "  Partitioned (odds first): ";
    for (int n : nums) std::cout << n << " ";
    std::cout << "\n";
}

// ============================================================================
// Part 4: std::function — Type-Erased Callable
// ============================================================================

// When you need to store a callable or pass it through an interface
using Callback = std::function<void(const std::string&)>;

class EventEmitter {
public:
    void on(const std::string& event, Callback cb) {
        callbacks_.push_back({event, std::move(cb)});
    }

    void emit(const std::string& event, const std::string& data) {
        for (const auto& [name, cb] : callbacks_) {
            if (name == event) {
                cb(data);
            }
        }
    }

private:
    std::vector<std::pair<std::string, Callback>> callbacks_;
};

void demo_std_function() {
    std::cout << "\n=== std::function & Callbacks ===\n";

    EventEmitter emitter;

    // Register callbacks using lambdas
    emitter.on("error", [](const std::string& msg) {
        std::cout << "  ERROR: " << msg << "\n";
    });

    emitter.on("data", [](const std::string& msg) {
        std::cout << "  DATA: " << msg << "\n";
    });

    int error_count = 0;
    emitter.on("error", [&error_count](const std::string&) {
        ++error_count;
    });

    emitter.emit("data", "Block 0x1A3F read complete");
    emitter.emit("error", "Disk timeout on /dev/sda2");
    emitter.emit("error", "CRC mismatch on block 0x2B4E");
    std::cout << "  Total errors: " << error_count << "\n";
}

// ============================================================================
// Part 5: Generic Lambdas (C++14) & Template Lambdas (C++20)
// ============================================================================

void demo_generic_lambdas() {
    std::cout << "\n=== Generic Lambdas ===\n";

    // C++14: auto parameters make lambdas into templates
    auto print_any = [](const auto& x) {
        std::cout << "  " << x << "\n";
    };
    print_any(42);
    print_any(3.14);
    print_any(std::string("hello"));

    // C++20: Explicit template parameter
    auto size_of = []<typename T>(const T&) {
        return sizeof(T);
    };
    std::cout << "  sizeof(int)    = " << size_of(0) << "\n";
    std::cout << "  sizeof(double) = " << size_of(0.0) << "\n";

    // Useful pattern: generic comparator factory
    auto make_field_comparator = [](auto field_accessor) {
        return [field_accessor](const auto& a, const auto& b) {
            return field_accessor(a) < field_accessor(b);
        };
    };

    struct Employee {
        std::string name;
        int salary;
    };

    std::vector<Employee> team = {{"Alice", 120}, {"Bob", 95}, {"Carol", 110}};
    std::sort(team.begin(), team.end(),
              make_field_comparator([](const Employee& e) { return e.salary; }));
    for (const auto& e : team) {
        std::cout << "  " << e.name << ": $" << e.salary << "k\n";
    }
}

// ============================================================================
// Part 6: Common Pitfall — Dangling Reference Capture
// ============================================================================

std::function<int()> create_counter_WRONG() {
    int count = 0;
    // BUG: `count` is a local variable. This lambda captures it by reference.
    // When create_counter_WRONG returns, `count` is destroyed → dangling reference.
    // return [&count]() { return ++count; };  // UNDEFINED BEHAVIOR!

    // CORRECT: Capture by value, make lambda mutable
    return [count]() mutable { return ++count; };
}

std::function<int()> create_counter_RIGHT() {
    int count = 0;
    return [count]() mutable { return ++count; };
    // `mutable` lets us modify the captured-by-value copy
}

void demo_dangling() {
    std::cout << "\n=== Dangling Reference Pitfall ===\n";
    auto counter = create_counter_RIGHT();
    std::cout << "  " << counter() << "\n";  // 1
    std::cout << "  " << counter() << "\n";  // 2
    std::cout << "  " << counter() << "\n";  // 3
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_lambda_basics();
    demo_captures();
    demo_stl_algorithms();
    demo_std_function();
    demo_generic_lambdas();
    demo_dangling();
    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Lambdas are syntactic sugar for anonymous functor objects.
//    The compiler generates a class with operator() for you.
//
// 2. CAPTURE BY VALUE [=] for async/stored callbacks. Never capture by reference
//    if the lambda outlives the captured variable.
//
// 3. std::function has overhead (heap allocation, virtual call). For templates,
//    accept the lambda directly as a template parameter — zero overhead.
//    void sort(Iter begin, Iter end, Compare comp);  // comp is a template param
//
// 4. Use `mutable` when you need a by-value capture that changes across calls.
//
// 5. Move-capture (C++14) is essential when you want a lambda to own a
//    unique_ptr or other move-only type.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a `retry(int max_attempts, std::function<bool()> task)` function
//    that calls `task` up to `max_attempts` times until it returns true.
//    Use a lambda as the task that simulates a flaky network call.
//
// 2. Implement a simple `Pipeline` class where you can chain transformations:
//      Pipeline<int> p;
//      p.add([](int x) { return x * 2; });
//      p.add([](int x) { return x + 1; });
//      p.run(5);  // returns 11
//
// 3. Write a memoize function: `auto memoized = memoize(expensive_fn);`
//    Use a lambda with a captured std::unordered_map.
//
// 4. Create a lambda that captures a std::unique_ptr<std::string> by move.
//    Pass it to a function that accepts std::function<void()>. What happens?
// ============================================================================
