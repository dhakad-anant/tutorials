/**
 * Module 06 — Lesson 1: Error Handling Strategies
 *
 * WHY THIS MATTERS:
 *   Error handling is where most codebases diverge from "correct" to "a mess."
 *   Storage systems have strict requirements: errors must be handled, never
 *   silently swallowed, and the system must be resilient to partial failures.
 *
 * APPROACHES COVERED:
 *   1. Exceptions (standard C++ way)
 *   2. Error codes / std::error_code
 *   3. std::expected (C++23) / Result types
 *   4. When to use which
 *
 * COMPILE: g++ -std=c++23 -Wall -Wextra -Werror -o error_handling 01_error_handling.cpp
 *   (For C++20, comment out the std::expected section)
 */

#include <iostream>
#include <string>
#include <system_error>
#include <variant>
#include <optional>
#include <stdexcept>
#include <cerrno>
#include <cassert>
#include <vector>
#include <functional>

// ============================================================================
// Part 1: Exceptions — The Standard Way
// ============================================================================

/*
 * PROS: Clean happy path. Automatic propagation. Works with RAII.
 * CONS: Non-zero overhead. Invisible control flow. Some codebases disable them.
 *
 * RULES:
 * - Throw for truly exceptional conditions (disk failure, OOM, corrupted data)
 * - Catch at boundaries (API layers, request handlers)
 * - NEVER catch(...) and swallow silently
 * - Use specific exception types
 */

// Define domain-specific exceptions
class StorageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DiskFullError : public StorageError {
public:
    DiskFullError() : StorageError("Disk full") {}
};

class CorruptionError : public StorageError {
    uint64_t block_;
public:
    explicit CorruptionError(uint64_t block)
        : StorageError("Data corruption at block " + std::to_string(block)),
          block_(block) {}
    [[nodiscard]] uint64_t block() const { return block_; }
};

void write_block_throws(uint64_t block_id, const std::string& data) {
    if (data.empty()) {
        throw std::invalid_argument("Cannot write empty data");
    }
    if (block_id == 0xDEAD) {
        throw CorruptionError(block_id);
    }
    if (block_id == 0xFFFF) {
        throw DiskFullError();
    }
    std::cout << "  Wrote " << data.size() << " bytes to block " << block_id << "\n";
}

void demo_exceptions() {
    std::cout << "=== Exception Handling ===\n";

    // Pattern: catch specific, then general
    try {
        write_block_throws(0x1000, "data");    // OK
        write_block_throws(0xDEAD, "data");    // throws CorruptionError
    } catch (const CorruptionError& e) {
        std::cout << "  CORRUPTION: " << e.what() << " (block=" << e.block() << ")\n";
    } catch (const StorageError& e) {
        std::cout << "  STORAGE ERROR: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cout << "  GENERAL ERROR: " << e.what() << "\n";
    }
}

// ============================================================================
// Part 2: Error Codes — Explicit Error Handling
// ============================================================================

// Many production codebases (including many at storage companies) prefer
// error codes because: no hidden control flow, works in no-exception builds,
// and performance is predictable.

enum class IOError {
    Success = 0,
    NotFound,
    PermissionDenied,
    DiskFull,
    Corruption,
    Timeout,
};

std::string to_string(IOError err) {
    switch (err) {
        case IOError::Success:          return "Success";
        case IOError::NotFound:         return "Not Found";
        case IOError::PermissionDenied: return "Permission Denied";
        case IOError::DiskFull:         return "Disk Full";
        case IOError::Corruption:       return "Corruption";
        case IOError::Timeout:          return "Timeout";
    }
    return "Unknown";
}

struct ReadResult {
    IOError error;
    std::string data;  // only valid if error == Success
};

ReadResult read_block_errcode(uint64_t block_id) {
    if (block_id == 0) return {IOError::NotFound, {}};
    if (block_id == 0xDEAD) return {IOError::Corruption, {}};
    return {IOError::Success, "block_data_" + std::to_string(block_id)};
}

void demo_error_codes() {
    std::cout << "\n=== Error Codes ===\n";

    auto result = read_block_errcode(42);
    if (result.error == IOError::Success) {
        std::cout << "  Read: " << result.data << "\n";
    } else {
        std::cout << "  Error: " << to_string(result.error) << "\n";
    }

    // The problem: it's easy to forget to check!
    auto bad = read_block_errcode(0);
    // Oops, using bad.data without checking error — silent bug!
    // [[nodiscard]] on the function helps, but doesn't fully prevent this.
}

// ============================================================================
// Part 3: Result<T, E> Type — Best of Both Worlds
// ============================================================================

// C++23 has std::expected<T, E>. Here's a simplified version:

template <typename T, typename E>
class Result {
public:
    // Success factory
    static Result ok(T value) { return Result(std::move(value)); }
    // Error factory
    static Result err(E error) { return Result(std::move(error), false); }

    [[nodiscard]] bool is_ok() const { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool is_err() const { return !is_ok(); }

    // Access value (throws if error)
    [[nodiscard]] const T& value() const {
        if (is_err()) throw std::runtime_error("Result holds error");
        return std::get<T>(data_);
    }

    [[nodiscard]] T& value() {
        if (is_err()) throw std::runtime_error("Result holds error");
        return std::get<T>(data_);
    }

    // Access error (throws if ok)
    [[nodiscard]] const E& error() const {
        if (is_ok()) throw std::runtime_error("Result holds value");
        return std::get<E>(data_);
    }

    // Monadic operations
    template <typename Func>
    auto map(Func&& f) const -> Result<decltype(f(std::declval<T>())), E> {
        if (is_ok()) return Result<decltype(f(value())), E>::ok(f(value()));
        return Result<decltype(f(value())), E>::err(error());
    }

    // value_or: return value if ok, otherwise default
    [[nodiscard]] T value_or(T default_val) const {
        if (is_ok()) return value();
        return default_val;
    }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    Result(E error, bool) : data_(std::move(error)) {}

    std::variant<T, E> data_;
};

Result<std::string, IOError> read_block_result(uint64_t block_id) {
    if (block_id == 0) return Result<std::string, IOError>::err(IOError::NotFound);
    if (block_id == 0xDEAD) return Result<std::string, IOError>::err(IOError::Corruption);
    return Result<std::string, IOError>::ok("block_" + std::to_string(block_id));
}

void demo_result_type() {
    std::cout << "\n=== Result<T, E> ===\n";

    auto r1 = read_block_result(42);
    if (r1.is_ok()) {
        std::cout << "  Read: " << r1.value() << "\n";
    }

    auto r2 = read_block_result(0);
    if (r2.is_err()) {
        std::cout << "  Error: " << to_string(r2.error()) << "\n";
    }

    // value_or for defaults
    auto data = read_block_result(0xDEAD).value_or("fallback_data");
    std::cout << "  With fallback: " << data << "\n";

    // map: transform the success value
    auto upper = read_block_result(42).map([](const std::string& s) {
        std::string result = s;
        for (auto& c : result) c = static_cast<char>(std::toupper(c));
        return result;
    });
    if (upper.is_ok()) {
        std::cout << "  Mapped: " << upper.value() << "\n";
    }
}

// ============================================================================
// Part 4: When to Use What
// ============================================================================

/*
 * USE EXCEPTIONS WHEN:
 *   - Error is truly exceptional (corruption, OOM, network down)
 *   - Error must propagate through multiple call layers
 *   - You're writing library code that doesn't know how the caller handles errors
 *   - RAII cleanup is needed on the error path
 *
 * USE ERROR CODES / Result<T,E> WHEN:
 *   - Errors are EXPECTED (file not found, retry needed)
 *   - Performance matters (hot path, real-time)
 *   - The codebase disables exceptions (-fno-exceptions)
 *   - You want the caller to explicitly handle every error
 *
 * THE PATTERN AT SCALE:
 *   - Low-level (disk I/O, network): error codes or Result<T,E>
 *   - Mid-level (request handler): translate to exceptions at boundaries
 *   - High-level (API): catch exceptions, return user-facing errors
 *
 * PURE STORAGE LIKELY DOES:
 *   - No exceptions in hot I/O paths (predictable latency)
 *   - Error codes or Result types for I/O operations
 *   - Maybe exceptions for configuration, initialization, RPC layers
 */

// ============================================================================
// Part 5: [[nodiscard]] and Compile-Time Error Prevention
// ============================================================================

// Force callers to check error results
[[nodiscard("Check the error!")]]
IOError dangerous_operation() {
    return IOError::DiskFull;
}

// Structured error context
struct ErrorContext {
    IOError     code;
    std::string message;
    std::string file;
    int         line;
};

#define MAKE_ERROR(code, msg) \
    ErrorContext{code, msg, __FILE__, __LINE__}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_exceptions();
    demo_error_codes();
    demo_result_type();

    std::cout << "\n=== [[nodiscard]] ===\n";
    // dangerous_operation();  // WARNING: ignoring return value!
    auto err = dangerous_operation();
    std::cout << "  Operation result: " << to_string(err) << "\n";

    auto ctx = MAKE_ERROR(IOError::Timeout, "disk did not respond");
    std::cout << "  Error at " << ctx.file << ":" << ctx.line
              << " — " << ctx.message << "\n";

    return 0;
}

// ============================================================================
// EXERCISES:
//
// 1. Implement a retry wrapper:
//      Result<T,E> retry(int max, std::function<Result<T,E>()> op)
//    It should retry on Timeout errors, give up on others.
//
// 2. Create an error_chain helper that wraps an inner error with context:
//      "Failed to read superblock: Disk timeout on /dev/sda1"
//
// 3. Add logging to the exception handlers: log the exception type, message,
//    and source location (std::source_location in C++20).
//
// 4. Benchmark: exception throw+catch vs returning Result<T,E> for 1M operations.
//    How much slower are exceptions on the error path? On the happy path?
// ============================================================================
