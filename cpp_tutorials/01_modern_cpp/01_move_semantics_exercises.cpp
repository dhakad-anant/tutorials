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

// Forward declaration for exercise solutions (defined below main)
int exercise_main();

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

    // Run exercise solutions
    exercise_main();

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
// EXERCISE SOLUTIONS
// ============================================================================

// ---------------------------------------------------------------------------
// Exercise 1: Add write() method, verify moved-from buffer is empty
// ---------------------------------------------------------------------------
//
// EXPLANATION:
//   write() copies a C-string into the buffer's internal data_. After a move,
//   data_ is nullptr and size_ is 0 in the source, so calling write() on a
//   moved-from buffer would be invalid (we guard against it). This proves
//   that move truly transferred ownership — the source has nothing left.
//
// NOTE: We need to add write() and a data accessor to the Buffer class above.
//   For a self-contained demo, we redefine a slightly extended Buffer here.

class ExBuffer {
public:
    explicit ExBuffer(size_t size)
        : size_(size), data_(new char[size]) {
        std::memset(data_, 0, size_);
    }
    ~ExBuffer() { delete[] data_; }

    // Copy
    ExBuffer(const ExBuffer& o) : size_(o.size_), data_(new char[o.size_]) {
        std::memcpy(data_, o.data_, size_);
    }
    ExBuffer& operator=(const ExBuffer& o) {
        if (this != &o) { delete[] data_; size_ = o.size_; data_ = new char[size_]; std::memcpy(data_, o.data_, size_); }
        return *this;
    }

    // Move
    ExBuffer(ExBuffer&& o) noexcept
        : size_(std::exchange(o.size_, 0)), data_(std::exchange(o.data_, nullptr)) {}
    ExBuffer& operator=(ExBuffer&& o) noexcept {
        if (this != &o) { delete[] data_; size_ = std::exchange(o.size_, 0); data_ = std::exchange(o.data_, nullptr); }
        return *this;
    }

    // --- Exercise 1: write() ---
    // Copies msg into data_. Truncates if msg is longer than buffer size.
    void write(const char* msg) {
        if (!data_ || size_ == 0) {
            std::cout << "  [write] ERROR: buffer is empty (moved-from?)\n";
            return;
        }
        size_t len = std::strlen(msg);
        size_t to_copy = (len < size_) ? len : size_ - 1;  // leave room for '\0'
        std::memcpy(data_, msg, to_copy);
        data_[to_copy] = '\0';
    }

    // Read back the data as a C-string (for verification)
    [[nodiscard]] const char* data() const { return data_ ? data_ : "(null)"; }
    [[nodiscard]] size_t size() const { return size_; }

private:
    size_t size_;
    char*  data_;
};

void exercise1() {
    std::cout << "\n========== EXERCISE 1: write() + verify move ==========\n";

    ExBuffer buf(64);
    buf.write("Hello, move semantics!");
    std::cout << "  Before move: data = \"" << buf.data() << "\"\n";

    ExBuffer buf2 = std::move(buf);  // move — steal buf's guts
    std::cout << "  After move (source):  data = \"" << buf.data()
              << "\", size = " << buf.size() << "\n";
    std::cout << "  After move (target):  data = \"" << buf2.data()
              << "\", size = " << buf2.size() << "\n";

    // Try writing to the moved-from buffer — should fail gracefully
    buf.write("This should fail");
}

// ---------------------------------------------------------------------------
// Exercise 2: noexcept and std::vector behavior
// ---------------------------------------------------------------------------
//
// EXPLANATION:
//   std::vector::push_back uses move IF the move constructor is noexcept.
//   If it's not noexcept, vector falls back to COPYING for strong exception
//   safety (if a move throws mid-reallocation, the original data is gone and
//   you can't roll back — so vector plays it safe and copies instead).
//
//   We demonstrate this with two classes:
//   - MoveNoexcept   (move ctor is noexcept) → vector uses move
//   - MoveThrows     (move ctor is NOT noexcept) → vector uses copy
//
//   To observe the effect, do NOT call reserve() so that push_back triggers
//   reallocation and has to relocate existing elements.

class MoveNoexcept {
public:
    explicit MoveNoexcept(int id) : id_(id) {}
    MoveNoexcept(const MoveNoexcept& o) : id_(o.id_) { std::cout << "  [MoveNoexcept] COPIED id=" << id_ << "\n"; }
    MoveNoexcept(MoveNoexcept&& o) noexcept : id_(o.id_) { o.id_ = -1; std::cout << "  [MoveNoexcept] MOVED id=" << id_ << "\n"; }
    MoveNoexcept& operator=(const MoveNoexcept&) = default;
    MoveNoexcept& operator=(MoveNoexcept&&) noexcept = default;
private:
    int id_;
};

class MoveThrows {
public:
    explicit MoveThrows(int id) : id_(id) {}
    MoveThrows(const MoveThrows& o) : id_(o.id_) { std::cout << "  [MoveThrows]   COPIED id=" << id_ << "\n"; }
    // NO noexcept — vector will refuse to move during reallocation
    MoveThrows(MoveThrows&& o) : id_(o.id_) { o.id_ = -1; std::cout << "  [MoveThrows]   MOVED id=" << id_ << "\n"; }
    MoveThrows& operator=(const MoveThrows&) = default;
    MoveThrows& operator=(MoveThrows&&) = default;
private:
    int id_;
};

void exercise2() {
    std::cout << "\n========== EXERCISE 2: noexcept effect on vector ==========\n";

    std::cout << "--- With noexcept (should MOVE during reallocation) ---\n";
    {
        std::vector<MoveNoexcept> vec; // no reserve — force reallocation
        vec.push_back(MoveNoexcept(1));
        std::cout << "  (reallocation happens on next push_back)\n";
        vec.push_back(MoveNoexcept(2)); // existing element #1 gets MOVED
    }

    std::cout << "\n--- Without noexcept (should COPY during reallocation) ---\n";
    {
        std::vector<MoveThrows> vec; // no reserve — force reallocation
        vec.push_back(MoveThrows(1));
        std::cout << "  (reallocation happens on next push_back)\n";
        vec.push_back(MoveThrows(2)); // existing element #1 gets COPIED (not moved!)
    }
}

// ---------------------------------------------------------------------------
// Exercise 3: Buffer merge(Buffer a, Buffer b)
// ---------------------------------------------------------------------------
//
// EXPLANATION:
//   Parameters are taken BY VALUE on purpose. This means:
//   - If the caller passes an lvalue  → it gets COPIED into the parameter
//   - If the caller passes an rvalue  → it gets MOVED into the parameter
//   The function doesn't care — it consumes a and b, merging their contents.
//
//   Taking by value lets the CALLER decide (via std::move) whether to give up
//   ownership. The function itself is clean and doesn't need separate
//   overloads for const& vs &&. This is the "sink parameter" idiom.

ExBuffer merge(ExBuffer a, ExBuffer b) {
    ExBuffer result(a.size() + b.size());
    // We need raw access; for the exercise we write sequentially.
    // Since ExBuffer::write() null-terminates, we do a small workaround:
    // write a's content, then append b's content manually.
    // A real implementation would expose data_ or provide an append method.
    // Here we demonstrate the move semantics aspect — a and b are consumed.

    // For simplicity, just write a combined message:
    std::string combined = std::string(a.data()) + std::string(b.data());
    result.write(combined.c_str());
    return result;  // NRVO will likely elide the move
}

void exercise3() {
    std::cout << "\n========== EXERCISE 3: merge(Buffer, Buffer) ==========\n";

    ExBuffer x(32);
    x.write("Hello ");
    ExBuffer y(32);
    y.write("World!");

    std::cout << "  Before merge: x=\"" << x.data() << "\", y=\"" << y.data() << "\"\n";

    // Pass x by copy (keep it), pass y by move (give it up)
    ExBuffer merged = merge(x, std::move(y));
    std::cout << "  Merged: \"" << merged.data() << "\" (" << merged.size() << " bytes)\n";
    std::cout << "  After merge: x=\"" << x.data() << "\" (still valid, was copied in)\n";
    std::cout << "  After merge: y.size()=" << y.size() << " (moved-from)\n";
}

// ---------------------------------------------------------------------------
// Exercise 4: BufferPool with acquire() and release()
// ---------------------------------------------------------------------------
//
// EXPLANATION:
//   BufferPool owns a vector of pre-allocated buffers. Clients call:
//     - acquire() → moves a buffer OUT of the pool (O(1), no copy)
//     - release(Buffer) → moves a buffer BACK into the pool
//
//   This is a simplified object pool pattern used in systems code to avoid
//   repeated allocation/deallocation. The key insight is that acquire()
//   returns by value, which triggers move construction. release() takes by
//   value (sink parameter), so the caller can std::move() in.
//
//   We use std::optional for acquire() to handle the empty-pool case cleanly.

#include <optional>

class BufferPool {
public:
    // Pre-populate the pool with `count` buffers of `buf_size` bytes
    BufferPool(size_t count, size_t buf_size) {
        pool_.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            pool_.emplace_back(buf_size);
        }
        std::cout << "  [Pool] Created with " << count << " buffers of "
                  << buf_size << " bytes each\n";
    }

    // Move a buffer out of the pool. Returns nullopt if pool is empty.
    std::optional<ExBuffer> acquire() {
        if (pool_.empty()) {
            std::cout << "  [Pool] acquire() — pool is EMPTY!\n";
            return std::nullopt;
        }
        // Move the last buffer out (O(1) — no shifting needed)
        ExBuffer buf = std::move(pool_.back());
        pool_.pop_back();
        std::cout << "  [Pool] acquire() — gave out buffer of "
                  << buf.size() << " bytes. " << pool_.size() << " remaining.\n";
        return buf;  // moved out via NRVO or move ctor
    }

    // Move a buffer back into the pool
    void release(ExBuffer buf) {
        std::cout << "  [Pool] release() — returned buffer of "
                  << buf.size() << " bytes.\n";
        pool_.push_back(std::move(buf));  // move into vector
        std::cout << "  [Pool] Pool now has " << pool_.size() << " buffers.\n";
    }

    [[nodiscard]] size_t available() const { return pool_.size(); }

private:
    std::vector<ExBuffer> pool_;
};

void exercise4() {
    std::cout << "\n========== EXERCISE 4: BufferPool ==========\n";

    BufferPool pool(3, 256);
    std::cout << "  Available: " << pool.available() << "\n";

    // Acquire two buffers
    auto buf1 = pool.acquire();
    auto buf2 = pool.acquire();
    std::cout << "  Available after 2 acquires: " << pool.available() << "\n";

    // Use them
    if (buf1) buf1->write("I'm buffer 1");
    if (buf2) buf2->write("I'm buffer 2");

    // Release one back
    if (buf1) pool.release(std::move(*buf1));
    std::cout << "  Available after 1 release: " << pool.available() << "\n";

    // Acquire until empty
    auto buf3 = pool.acquire();
    auto buf4 = pool.acquire();
    auto buf5 = pool.acquire(); // should fail — pool is empty

    // Release everything back
    if (buf2) pool.release(std::move(*buf2));
    if (buf3) pool.release(std::move(*buf3));
    if (buf4) pool.release(std::move(*buf4));
    std::cout << "  Final available: " << pool.available() << "\n";
}

// ============================================================================
// Run all exercises
// ============================================================================

int exercise_main() {
    exercise1();
    exercise2();
    exercise3();
    exercise4();
    return 0;
}
