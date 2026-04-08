/**
 * Module 01 — Lesson 1: Move Semantics & Rvalue References
 *
 * WHY THIS MATTERS:
 *   In production code (e.g., at Pure Storage), objects like buffers, strings,
 *   and containers are frequently passed around. Copying a 64 MB buffer when you
 *   could MOVE it is a performance disaster. Move semantics let you "steal" the
 *   guts of a temporary object instead of copying them.
 *
 * PREREQUISITES: You know what copy constructors and assignment operators are.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o move_semantics 01_move_semantics.cpp
 */

#include <iostream>
#include <cstring>
#include <utility>  // std::move, std::exchange
#include <vector>

// ============================================================================
// Part 1: Understanding lvalues vs rvalues
// ============================================================================

/*
 * LVALUE: Has a name, has an address. You can take &x.
 *   int x = 42;       // x is an lvalue
 *   std::string s;     // s is an lvalue
 *
 * RVALUE: A temporary. No name you can refer to after the expression.
 *   42                 // rvalue
 *   x + y              // rvalue (the result)
 *   std::string("hi")  // rvalue (temporary)
 *   std::move(s)       // rvalue (cast — s is still alive, but you promised not to use it)
 *
 * WHY IT MATTERS:
 *   If something is an rvalue (temporary, about to die), we can STEAL its resources
 *   instead of copying them. That's what move semantics enable.
 */

// ============================================================================
// Part 2: A class with explicit copy and move operations
// ============================================================================

class Buffer {
public:
    // --- Constructor ---
    explicit Buffer(size_t size)
        : size_(size), data_(new char[size]) {
        std::memset(data_, 0, size_);
        std::cout << "  [Buffer] Constructed: " << size_ << " bytes\n";
    }

    // --- Destructor ---
    ~Buffer() {
        delete[] data_;
        std::cout << "  [Buffer] Destroyed (size was " << size_ << ")\n";
    }

    // --- Copy Constructor ---
    // Makes a deep copy. Expensive for large buffers.
    Buffer(const Buffer& other)
        : size_(other.size_), data_(new char[other.size_]) {
        std::memcpy(data_, other.data_, size_);
        std::cout << "  [Buffer] COPIED: " << size_ << " bytes (expensive!)\n";
    }

    // --- Copy Assignment ---
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            data_ = new char[size_];
            std::memcpy(data_, other.data_, size_);
            std::cout << "  [Buffer] COPY-ASSIGNED: " << size_ << " bytes\n";
        }
        return *this;
    }

    // --- Move Constructor ---
    // Steals the guts of `other`. O(1) instead of O(n).
    Buffer(Buffer&& other) noexcept
        : size_(std::exchange(other.size_, 0)),       // take size, zero theirs
          data_(std::exchange(other.data_, nullptr))   // take pointer, null theirs
    {
        std::cout << "  [Buffer] MOVED: " << size_ << " bytes (cheap!)\n";
    }

    // --- Move Assignment ---
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;                             // free our old data
            size_ = std::exchange(other.size_, 0);
            data_ = std::exchange(other.data_, nullptr);
            std::cout << "  [Buffer] MOVE-ASSIGNED: " << size_ << " bytes\n";
        }
        return *this;
    }

    [[nodiscard]] size_t size() const { return size_; }

private:
    size_t size_;
    char*  data_;
};

// ============================================================================
// Part 3: Seeing it in action
// ============================================================================

Buffer createBuffer(size_t sz) {
    Buffer b(sz);
    return b;  // NRVO (Named Return Value Optimization) may elide this move entirely.
               // But if it doesn't, the move constructor is called — not the copy.
}

void takeOwnership(Buffer buf) {
    std::cout << "  takeOwnership() got a buffer of " << buf.size() << " bytes\n";
    // buf is destroyed at end of scope
}

int main() {
    std::cout << "=== 1. Basic construction ===\n";
    Buffer a(1024);

    std::cout << "\n=== 2. Copy construction ===\n";
    Buffer b = a;  // Calls copy constructor — deep copy

    std::cout << "\n=== 3. Move construction with std::move ===\n";
    Buffer c = std::move(a);  // Calls move constructor — steals a's data
    // WARNING: 'a' is now in a valid-but-unspecified state. Don't use it.
    std::cout << "  a.size() after move: " << a.size() << " (should be 0)\n";

    std::cout << "\n=== 4. Return value (may be elided by NRVO) ===\n";
    Buffer d = createBuffer(2048);

    std::cout << "\n=== 5. Passing to a function by value (triggers move for rvalue) ===\n";
    takeOwnership(std::move(d));

    std::cout << "\n=== 6. Vector push_back: copy vs move ===\n";
    std::vector<Buffer> vec;
    vec.reserve(2);  // IMPORTANT: reserve to avoid reallocation-triggered moves
    Buffer e(512);
    vec.push_back(e);             // copy (e is an lvalue)
    vec.push_back(std::move(e));  // move (we cast e to rvalue)

    std::cout << "\n=== Cleanup ===\n";
    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. std::move() does NOT move anything. It's a CAST to rvalue reference.
//    The actual move happens in the move constructor/assignment.
//
// 2. After std::move(x), x is in a "valid but unspecified" state.
//    Only safe operations: destroy it, or assign a new value to it.
//
// 3. Mark move constructors and move assignments as `noexcept`.
//    - std::vector will ONLY use move if it's noexcept (otherwise it copies for safety).
//    - This is critical for performance.
//
// 4. Use std::exchange(old, new_value) in move operations — it's clean and clear.
//
// 5. The compiler can elide moves entirely via NRVO/RVO. Don't pessimize by
//    writing `return std::move(local);` — that PREVENTS NRVO.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Add a `write(const char* msg)` method to Buffer that copies msg into data_.
//    Verify that after moving, the source buffer's data is truly gone.
//
// 2. Remove `noexcept` from the move constructor. Put Buffer into a vector and
//    observe that push_back now COPIES. Re-add noexcept and confirm moves resume.
//
// 3. Write a function `Buffer merge(Buffer a, Buffer b)` that creates a new
//    buffer of size a.size() + b.size() and copies both. Think about the
//    parameter passing — why take by value here?
//
// 4. Create a `BufferPool` class that owns a vector<Buffer>. Implement
//    `acquire()` that moves a buffer out and `release(Buffer)` that moves one back.
// ============================================================================
