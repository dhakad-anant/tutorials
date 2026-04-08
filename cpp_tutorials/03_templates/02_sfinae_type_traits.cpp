/**
 * Module 03 — Lesson 2: SFINAE, Type Traits & Concepts
 *
 * WHY THIS MATTERS:
 *   SFINAE and type traits are how C++ templates selectively enable/disable
 *   overloads based on type properties. C++20 concepts replace SFINAE with
 *   readable syntax, but understanding SFINAE is essential for reading existing
 *   production codebases.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o sfinae 02_sfinae_type_traits.cpp
 */

#include <iostream>
#include <type_traits>
#include <string>
#include <vector>
#include <concepts>
#include <cstdint>

// ============================================================================
// Part 1: Type Traits — Compile-Time Type Queries
// ============================================================================

void demo_type_traits() {
    std::cout << "=== Type Traits ===\n";

    // is_*: query type properties
    std::cout << std::boolalpha;
    std::cout << "  is_integral<int>:        " << std::is_integral_v<int> << "\n";
    std::cout << "  is_integral<double>:     " << std::is_integral_v<double> << "\n";
    std::cout << "  is_floating_point<float>:" << std::is_floating_point_v<float> << "\n";
    std::cout << "  is_pointer<int*>:        " << std::is_pointer_v<int*> << "\n";
    std::cout << "  is_reference<int&>:      " << std::is_reference_v<int&> << "\n";
    std::cout << "  is_const<const int>:     " << std::is_const_v<const int> << "\n";
    std::cout << "  is_same<int, int32_t>:   " << std::is_same_v<int, int32_t> << "\n";

    // Transformation traits: modify types at compile time
    // remove_const_t<const int>  → int
    // remove_reference_t<int&>   → int
    // add_pointer_t<int>         → int*
    // decay_t<const int&>        → int (removes ref + const, like function param decay)

    using T1 = std::remove_const_t<const int>;          // int
    using T2 = std::remove_reference_t<int&&>;           // int
    using T3 = std::decay_t<const int(&)[10]>;           // const int*
    static_assert(std::is_same_v<T1, int>);
    static_assert(std::is_same_v<T2, int>);
}

// ============================================================================
// Part 2: SFINAE — Substitution Failure Is Not An Error
// ============================================================================

// The idea: if substituting a type into a template causes an error,
// the compiler silently ignores that overload instead of failing.

// SFINAE with enable_if: enable this function ONLY for integral types
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
safe_divide(T a, T b) {
    if (b == 0) return 0;  // safe default for integers
    return a / b;
}

// For floating point — different behavior
template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, T>
safe_divide(T a, T b) {
    if (b == 0.0) return std::numeric_limits<T>::quiet_NaN();
    return a / b;
}

// ============================================================================
// Part 3: Detection Idiom — Check if a Type Has a Method
// ============================================================================

// Pre-C++20: use SFINAE to detect if T has a .serialize() method
template <typename T, typename = void>
struct has_serialize : std::false_type {};

template <typename T>
struct has_serialize<T, std::void_t<decltype(std::declval<T>().serialize())>>
    : std::true_type {};

// Usage:
struct Serializable {
    std::string serialize() const { return "data"; }
};

struct NotSerializable {
    int value;
};

static_assert(has_serialize<Serializable>::value);
static_assert(!has_serialize<NotSerializable>::value);

// ============================================================================
// Part 4: C++20 Concepts Replace SFINAE
// ============================================================================

// BEFORE (SFINAE — hard to read):
// template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
// void process(T value);

// AFTER (Concepts — clear and clean):
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept HasSerialize = requires(T t) {
    { t.serialize() } -> std::convertible_to<std::string>;
};

template <typename T>
concept Printable = requires(T t, std::ostream& os) {
    { os << t } -> std::same_as<std::ostream&>;
};

// Use concepts directly
template <Arithmetic T>
T clamp_value(T value, T low, T high) {
    return (value < low) ? low : (value > high) ? high : value;
}

template <HasSerialize T>
void write_to_disk(const T& obj) {
    auto data = obj.serialize();
    std::cout << "  Writing: " << data << "\n";
}

// ============================================================================
// Part 5: Compile-Time Dispatch with if constexpr
// ============================================================================

// The modern replacement for SFINAE-heavy dispatch

template <typename T>
std::string to_debug_string(const T& value) {
    if constexpr (std::is_integral_v<T>) {
        return "int(" + std::to_string(value) + ")";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "float(" + std::to_string(value) + ")";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "str(\"" + value + "\")";
    } else if constexpr (HasSerialize<T>) {
        return "serializable(" + value.serialize() + ")";
    } else {
        return "unknown";
    }
}

// ============================================================================
// Part 6: Practical Example — Serialization Framework
// ============================================================================

// Write different types to a byte buffer

class ByteBuffer {
public:
    template <typename T>
    void write(const T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            // POD types: write raw bytes
            const auto* bytes = reinterpret_cast<const char*>(&value);
            data_.insert(data_.end(), bytes, bytes + sizeof(T));
            std::cout << "  Wrote " << sizeof(T) << " bytes (arithmetic)\n";
        } else if constexpr (std::is_same_v<T, std::string>) {
            // String: write length prefix + data
            uint32_t len = static_cast<uint32_t>(value.size());
            write(len);
            data_.insert(data_.end(), value.begin(), value.end());
            std::cout << "  Wrote string of length " << len << "\n";
        } else if constexpr (HasSerialize<T>) {
            // Custom types: call serialize
            auto serialized = value.serialize();
            write(serialized);
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type for ByteBuffer::write");
        }
    }

    [[nodiscard]] size_t size() const { return data_.size(); }

private:
    std::vector<char> data_;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_type_traits();

    std::cout << "\n=== SFINAE: safe_divide ===\n";
    std::cout << "  10 / 3 = " << safe_divide(10, 3) << "\n";
    std::cout << "  10 / 0 = " << safe_divide(10, 0) << "\n";
    std::cout << "  10.0 / 3.0 = " << safe_divide(10.0, 3.0) << "\n";

    std::cout << "\n=== Concepts ===\n";
    std::cout << "  clamp(15, 0, 10) = " << clamp_value(15, 0, 10) << "\n";
    Serializable obj;
    write_to_disk(obj);

    std::cout << "\n=== Compile-Time Dispatch ===\n";
    std::cout << "  " << to_debug_string(42) << "\n";
    std::cout << "  " << to_debug_string(3.14) << "\n";
    std::cout << "  " << to_debug_string(std::string("hello")) << "\n";
    std::cout << "  " << to_debug_string(obj) << "\n";

    std::cout << "\n=== ByteBuffer Serializer ===\n";
    ByteBuffer buf;
    buf.write(42);
    buf.write(3.14);
    buf.write(std::string("hello"));
    buf.write(obj);
    std::cout << "  Total buffer size: " << buf.size() << " bytes\n";

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Type traits (type_traits header) = compile-time type queries and transforms.
// 2. SFINAE = overload is silently removed if substitution fails. Powerful but ugly.
// 3. C++20 Concepts = clean, readable SFINAE replacement. Use them in new code.
// 4. if constexpr = compile-time branching that eliminates dead branches entirely.
// 5. enable_if → concepts. void_t detection → requires expressions. Know both.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a concept `Iterable` that requires begin(), end(), and that the
//    iterator supports ++ and *. Test it with vector, array, and int.
//
// 2. Using if constexpr and type traits, write a single `hash_value(T)` function
//    that: hashes integers by bit mixing, hashes strings with FNV-1a, and
//    refuses to compile for unsupported types.
//
// 3. Write a SFINAE-based `has_size_method<T>` detector (pre-C++20 style).
//    Then rewrite it as a C++20 concept. Compare the clarity.
//
// 4. Extend ByteBuffer to support reading (deserialization). Write<T> should be
//    symmetrical: read<int>(), read<string>(), etc.
// ============================================================================
