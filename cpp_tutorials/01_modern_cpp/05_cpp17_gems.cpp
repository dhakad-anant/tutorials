/**
 * Module 01 — Lesson 5: C++17 Gems
 *   std::optional, std::variant, std::string_view, structured bindings,
 *   std::any, fold expressions
 *
 * WHY THIS MATTERS:
 *   These features replace clunky patterns (sentinel values, unions, char* vs string,
 *   std::pair with .first/.second). Production code at Pure Storage will use these daily.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o cpp17_gems 05_cpp17_gems.cpp
 */

#include <iostream>
#include <optional>
#include <variant>
#include <string_view>
#include <string>
#include <vector>
#include <map>
#include <any>
#include <cstdint>

// ============================================================================
// Part 1: std::optional — "Maybe a value"
// ============================================================================

// BEFORE C++17: Return a bool + out-parameter, or use a sentinel like -1 or nullptr
// AFTER C++17:  Return std::optional<T>

struct DiskBlock {
    uint64_t address;
    uint32_t size;
    bool     compressed;
};

// Returns std::nullopt if the block is not found
std::optional<DiskBlock> find_block(uint64_t address) {
    // Simulated lookup
    if (address == 0x1A3F) {
        return DiskBlock{0x1A3F, 4096, true};
    }
    return std::nullopt;  // not found
}

void demo_optional() {
    std::cout << "=== std::optional ===\n";

    // Pattern 1: Check with has_value() or bool conversion
    auto result = find_block(0x1A3F);
    if (result) {
        std::cout << "  Found block at 0x" << std::hex << result->address
                  << ", size=" << std::dec << result->size << "\n";
    }

    // Pattern 2: value_or() for defaults
    auto missing = find_block(0xDEAD);
    DiskBlock fallback{0, 0, false};
    auto block = missing.value_or(fallback);
    std::cout << "  Missing block defaulted to size=" << block.size << "\n";

    // Pattern 3: Use with monadic operations (C++23, but worth knowing)
    // result.transform([](auto& b) { return b.size; });  // C++23
    // result.and_then([](auto& b) -> std::optional<int> { ... });  // C++23

    // COMMON BUG: Calling .value() on a nullopt throws std::bad_optional_access
    try {
        auto bad = find_block(0xDEAD);
        [[maybe_unused]] auto val = bad.value();  // throws!
    } catch (const std::bad_optional_access& e) {
        std::cout << "  Caught: " << e.what() << "\n";
    }
}

// ============================================================================
// Part 2: std::variant — Type-Safe Union
// ============================================================================

// Replaces C unions and error-prone type tags
// A variant holds EXACTLY ONE of the listed types at a time

using IOResult = std::variant<DiskBlock, std::string>;  // success OR error message

IOResult perform_io(bool succeed) {
    if (succeed) {
        return DiskBlock{0x2B4E, 8192, false};
    }
    return std::string("I/O error: device not ready");
}

// The Visitor pattern — cleanest way to handle variants
struct IOResultVisitor {
    void operator()(const DiskBlock& block) {
        std::cout << "  Success: block at 0x" << std::hex << block.address
                  << " (" << std::dec << block.size << " bytes)\n";
    }
    void operator()(const std::string& error) {
        std::cout << "  Error: " << error << "\n";
    }
};

void demo_variant() {
    std::cout << "\n=== std::variant ===\n";

    // Using std::visit with a visitor struct
    auto ok = perform_io(true);
    auto err = perform_io(false);
    std::visit(IOResultVisitor{}, ok);
    std::visit(IOResultVisitor{}, err);

    // Using std::visit with an overloaded lambda (requires a helper)
    // This is the modern idiomatic pattern:
    struct overloaded : IOResultVisitor {};  // can also use template trick below

    // Check which type is active
    std::cout << "  ok holds DiskBlock? " << std::boolalpha
              << std::holds_alternative<DiskBlock>(ok) << "\n";

    // Get the value (throws std::bad_variant_access if wrong type)
    auto& block = std::get<DiskBlock>(ok);
    std::cout << "  Block size: " << block.size << "\n";

    // Safe get with get_if (returns pointer or nullptr)
    if (auto* blk = std::get_if<DiskBlock>(&ok)) {
        std::cout << "  get_if succeeded: " << blk->size << "\n";
    }
}

// ============================================================================
// Part 3: Overloaded Lambda Pattern (for std::visit)
// ============================================================================

// This helper lets you combine multiple lambdas into one visitor
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// C++17 deduction guide:
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

void demo_overloaded_visit() {
    std::cout << "\n=== Overloaded Lambda Visit ===\n";

    using Value = std::variant<int, double, std::string>;
    std::vector<Value> values = {42, 3.14, std::string("hello"), 7};

    for (const auto& v : values) {
        std::visit(overloaded{
            [](int i)                { std::cout << "  int: " << i << "\n"; },
            [](double d)             { std::cout << "  double: " << d << "\n"; },
            [](const std::string& s) { std::cout << "  string: " << s << "\n"; },
        }, v);
    }
}

// ============================================================================
// Part 4: std::string_view — Non-Owning String Reference
// ============================================================================

// string_view is to string what a pointer is to an object:
// it doesn't own the data, it just looks at it.

// BEFORE: void process(const std::string& s) — forces allocation if called with char*
// AFTER:  void process(std::string_view s)   — zero-copy for any string-like thing

void parse_header(std::string_view header) {
    auto colon = header.find(':');
    if (colon == std::string_view::npos) {
        std::cout << "  Invalid header: " << header << "\n";
        return;
    }
    auto key = header.substr(0, colon);
    auto value = header.substr(colon + 2);  // skip ": "
    std::cout << "  Key: [" << key << "] Value: [" << value << "]\n";
}

void demo_string_view() {
    std::cout << "\n=== std::string_view ===\n";

    // Works with string literals (no allocation!)
    parse_header("Content-Type: application/octet-stream");

    // Works with std::string
    std::string s = "X-Block-Size: 4096";
    parse_header(s);

    // Works with substrings (zero-copy)
    std::string full = "GET /api/blocks HTTP/1.1\r\nHost: pure.storage\r\n";
    std::string_view sv(full);
    auto line_end = sv.find("\r\n");
    std::cout << "  First line: " << sv.substr(0, line_end) << "\n";

    // DANGER: string_view does NOT own the data!
    // If the underlying string is destroyed, the view is dangling.
    // NEVER return a string_view to a local string.
}

// ============================================================================
// Part 5: Structured Bindings
// ============================================================================

void demo_structured_bindings() {
    std::cout << "\n=== Structured Bindings ===\n";

    // With pairs (replaces .first/.second ugliness)
    std::map<std::string, int> disk_usage = {
        {"/dev/sda1", 85}, {"/dev/sdb1", 42}, {"/dev/nvme0n1", 97}
    };

    for (const auto& [device, pct] : disk_usage) {
        std::cout << "  " << device << ": " << pct << "%"
                  << (pct > 90 ? " ⚠ WARNING" : "") << "\n";
    }

    // With insert results
    auto [iter, inserted] = disk_usage.insert({"/dev/sdc1", 15});
    std::cout << "  Inserted /dev/sdc1? " << std::boolalpha << inserted << "\n";

    // With structs
    DiskBlock block{0xABCD, 4096, true};
    auto [addr, sz, compressed] = block;
    std::cout << "  Block: addr=0x" << std::hex << addr
              << " size=" << std::dec << sz
              << " compressed=" << compressed << "\n";

    // With arrays
    int coords[] = {10, 20, 30};
    auto [x, y, z] = coords;
    std::cout << "  Coords: " << x << ", " << y << ", " << z << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_optional();
    demo_variant();
    demo_overloaded_visit();
    demo_string_view();
    demo_structured_bindings();
    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. optional<T>    — replaces sentinel values, null pointers, bool+out-param
// 2. variant<Ts...> — replaces unions, safer than polymorphism for closed type sets
// 3. string_view    — zero-copy string reference; never outlive the source
// 4. Structured bindings — readable decomposition of pairs, tuples, structs
// 5. The overloaded{} pattern for std::visit is THE idiomatic way to handle variants
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a config parser that returns std::optional<std::string> for each key.
//    Chain lookups: find key in env vars, then config file, then default.
//
// 2. Model a JSON value as:
//      using JsonValue = std::variant<std::nullptr_t, bool, int, double,
//                                     std::string, std::vector<JsonValue>,
//                                     std::map<std::string, JsonValue>>;
//    Write a pretty-printer using std::visit + overloaded{}.
//
// 3. Write a function tokenize(std::string_view input) -> std::vector<std::string_view>
//    that splits input on spaces WITHOUT any allocations (returns views into input).
//
// 4. Refactor the DiskBlock lookup to use variant<DiskBlock, ErrorCode> instead
//    of optional, where ErrorCode is an enum {NotFound, PermissionDenied, IOError}.
// ============================================================================
