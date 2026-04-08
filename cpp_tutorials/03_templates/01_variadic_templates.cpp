/**
 * Module 03 — Lesson 1: Variadic Templates & Fold Expressions
 *
 * WHY THIS MATTERS:
 *   Variadic templates let you write functions and classes that accept ANY number
 *   of arguments of ANY types. They power std::tuple, std::variant, logging libraries,
 *   serialization frameworks, and printf-style APIs.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o variadic 01_variadic_templates.cpp
 */

#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <sstream>
#include <memory>
#include <utility>

// ============================================================================
// Part 1: Basic Variadic Function Templates
// ============================================================================

// Base case (no arguments)
void print() {
    std::cout << "\n";
}

// Recursive case: peel off one argument at a time
template <typename T, typename... Rest>
void print(const T& first, const Rest&... rest) {
    std::cout << first;
    if constexpr (sizeof...(rest) > 0) {
        std::cout << ", ";
    }
    print(rest...);   // recurse with remaining arguments
}

// ============================================================================
// Part 2: Fold Expressions (C++17) — Replace Recursion
// ============================================================================

// Fold expressions apply an operator across a parameter pack in one line
// Four forms:
//   (pack op ...)       — right fold:  (a op (b op (c op d)))
//   (... op pack)       — left fold:   (((a op b) op c) op d)
//   (pack op ... op init) — right fold with init
//   (init op ... op pack) — left fold with init

template <typename... Args>
auto sum(Args... args) {
    return (args + ...);  // right fold: (a + (b + (c + d)))
}

template <typename... Args>
auto product(Args... args) {
    return (args * ...);  // right fold
}

template <typename... Args>
bool all_true(Args... args) {
    return (args && ...);  // right fold with &&
}

template <typename... Args>
bool any_true(Args... args) {
    return (args || ...);  // right fold with ||
}

// Print with fold expression (much cleaner than recursion)
template <typename... Args>
void print_fold(const Args&... args) {
    ((std::cout << args << " "), ...);  // comma fold: executes each expression
    std::cout << "\n";
}

// ============================================================================
// Part 3: Perfect Forwarding with Variadic Templates
// ============================================================================

// The make_unique pattern: forward any number of args to a constructor
template <typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// A logging function that forwards to std::ostringstream
template <typename... Args>
std::string format(const Args&... args) {
    std::ostringstream oss;
    ((oss << args), ...);  // fold expression
    return oss.str();
}

// ============================================================================
// Part 4: Variadic Class Templates
// ============================================================================

// Type-safe heterogeneous container (simplified std::tuple)
template <typename... Types>
struct TypeList {
    static constexpr size_t size = sizeof...(Types);
};

// Compile-time type checking
using MyTypes = TypeList<int, double, std::string>;
static_assert(MyTypes::size == 3);

// A practical example: multi-type event handler
template <typename... EventTypes>
class EventDispatcher {
public:
    // Register a handler for a specific event type
    template <typename Event, typename Handler>
    void on(Handler&& handler) {
        // In a real implementation, you'd store handlers in a type-indexed map
        // For now, we demonstrate the pattern:
        std::cout << "  Registered handler for event (size=" << sizeof(Event) << ")\n";
        // Store handler... (simplified)
        static_cast<void>(handler);
    }

    template <typename Event>
    void emit(const Event& event) {
        std::cout << "  Emitting event (size=" << sizeof(event) << ")\n";
    }
};

// ============================================================================
// Part 5: Index Sequences (for tuple operations)
// ============================================================================

// Print all elements of a tuple
template <typename Tuple, size_t... Is>
void print_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << (Is == 0 ? "" : ", ") << std::get<Is>(t)), ...);
}

template <typename... Args>
void print_tuple(const std::tuple<Args...>& t) {
    std::cout << "(";
    print_tuple_impl(t, std::index_sequence_for<Args...>{});
    std::cout << ")\n";
}

// Apply a function to each tuple element
template <typename Func, typename Tuple, size_t... Is>
void for_each_tuple_impl(Func&& f, const Tuple& t, std::index_sequence<Is...>) {
    (f(std::get<Is>(t)), ...);
}

template <typename Func, typename... Args>
void for_each_tuple(Func&& f, const std::tuple<Args...>& t) {
    for_each_tuple_impl(std::forward<Func>(f), t, std::index_sequence_for<Args...>{});
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Variadic Print (recursive) ===\n";
    print(1, 2.5, "hello", 'x');

    std::cout << "\n=== Fold Expressions ===\n";
    std::cout << "  sum(1,2,3,4,5) = " << sum(1, 2, 3, 4, 5) << "\n";
    std::cout << "  product(2,3,4)  = " << product(2, 3, 4) << "\n";
    std::cout << "  all_true(true, true, false) = "
              << std::boolalpha << all_true(true, true, false) << "\n";
    std::cout << "  any_true(false, false, true) = "
              << any_true(false, false, true) << "\n";
    std::cout << "  print_fold: ";
    print_fold(42, 3.14, "world");

    std::cout << "\n=== Perfect Forwarding ===\n";
    auto s = my_make_unique<std::string>("hello from my_make_unique");
    std::cout << "  " << *s << "\n";
    auto msg = format("Block ", 0x1A3F, " size=", 4096, " ok");
    std::cout << "  " << msg << "\n";

    std::cout << "\n=== Tuple Operations ===\n";
    auto t = std::make_tuple(42, 3.14, std::string("hello"));
    std::cout << "  Tuple: ";
    print_tuple(t);

    std::cout << "  Elements: ";
    for_each_tuple([](const auto& x) { std::cout << "[" << x << "] "; }, t);
    std::cout << "\n";

    std::cout << "\n=== Event Dispatcher ===\n";
    struct ClickEvent { int x, y; };
    struct KeyEvent { char key; };
    EventDispatcher<ClickEvent, KeyEvent> dispatcher;
    dispatcher.on<ClickEvent>([](const ClickEvent&) {});
    dispatcher.emit(ClickEvent{100, 200});

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Parameter packs (typename... Args) + pack expansion (args...) are the core.
// 2. sizeof...(pack) gives the count at compile time.
// 3. Fold expressions (C++17) replace recursive templates for simple operations.
// 4. std::forward<Args>(args)... = perfect forwarding through variadics.
// 5. std::index_sequence + std::get = iterate tuples at compile time.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a `min(args...)` function using fold expressions that returns the
//    minimum of any number of comparable arguments.
//
// 2. Write a type-safe `printf` replacement using variadic templates:
//      my_printf("name={} age={} score={}", "Alice", 30, 95.5);
//
// 3. Implement a compile-time `contains<T, Types...>()` that returns true
//    if T is one of the Types. Use fold expressions.
//
// 4. Write `zip_tuples(t1, t2)` that creates a tuple of pairs from two tuples:
//      zip({1,"a"}, {2,"b"}) → {{1,2}, {"a","b"}}
// ============================================================================
