/**
 * Module 01 — Lesson 2: Smart Pointers
 *
 * WHY THIS MATTERS:
 *   Raw `new`/`delete` is the #1 source of bugs in C++ codebases: leaks, double-frees,
 *   dangling pointers, use-after-free. Production code uses smart pointers exclusively.
 *   At Pure Storage, you'll likely never write `new`/`delete` directly.
 *
 * RULE OF THUMB:
 *   - std::unique_ptr — single owner (default choice, zero overhead)
 *   - std::shared_ptr — shared ownership (reference counted, small overhead)
 *   - std::weak_ptr   — non-owning observer of shared_ptr (breaks cycles)
 *   - Raw pointer      — non-owning, borrowed reference (never owns memory)
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o smart_pointers 02_smart_pointers.cpp
 */

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cassert>

// ============================================================================
// Part 1: unique_ptr — Single Ownership
// ============================================================================

class Connection {
public:
    explicit Connection(std::string name) : name_(std::move(name)) {
        std::cout << "  [Connection] Opened: " << name_ << "\n";
    }
    ~Connection() {
        std::cout << "  [Connection] Closed: " << name_ << "\n";
    }

    void send(const std::string& msg) {
        std::cout << "  [Connection] " << name_ << " sending: " << msg << "\n";
    }

    [[nodiscard]] const std::string& name() const { return name_; }

    // Delete copy to enforce single ownership
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Allow move
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

private:
    std::string name_;
};

void demo_unique_ptr() {
    std::cout << "=== unique_ptr Demo ===\n";

    // GOOD: Use make_unique (exception-safe, no raw new)
    auto conn = std::make_unique<Connection>("DB-Primary");
    conn->send("SELECT * FROM volumes");

    // Transfer ownership
    auto conn2 = std::move(conn);
    // conn is now nullptr
    assert(conn == nullptr);
    conn2->send("INSERT INTO snapshots ...");

    // unique_ptr in containers
    std::vector<std::unique_ptr<Connection>> pool;
    pool.push_back(std::make_unique<Connection>("DB-Replica-1"));
    pool.push_back(std::make_unique<Connection>("DB-Replica-2"));

    for (const auto& c : pool) {
        c->send("PING");
    }

    // Everything automatically cleaned up at scope exit
    std::cout << "  (leaving scope — watch destructors fire in reverse order)\n";
}

// ============================================================================
// Part 2: unique_ptr with Custom Deleters
// ============================================================================

// Simulates a C-style API (common when wrapping C libraries)
struct FileHandle {
    int fd;
    std::string path;
};

FileHandle* open_file(const char* path) {
    std::cout << "  [C API] Opening: " << path << "\n";
    return new FileHandle{42, path};
}

void close_file(FileHandle* fh) {
    std::cout << "  [C API] Closing: " << fh->path << " (fd=" << fh->fd << ")\n";
    delete fh;
}

void demo_custom_deleter() {
    std::cout << "\n=== Custom Deleter Demo ===\n";

    // Wrap a C-style resource in unique_ptr with a custom deleter
    auto file = std::unique_ptr<FileHandle, decltype(&close_file)>(
        open_file("/dev/nvme0n1"),
        &close_file
    );

    std::cout << "  Using file: " << file->path << "\n";
    // close_file called automatically when `file` goes out of scope
}

// ============================================================================
// Part 3: shared_ptr & weak_ptr
// ============================================================================

class CacheEntry {
public:
    explicit CacheEntry(std::string key) : key_(std::move(key)) {
        std::cout << "  [CacheEntry] Created: " << key_ << "\n";
    }
    ~CacheEntry() {
        std::cout << "  [CacheEntry] Evicted: " << key_ << "\n";
    }

    [[nodiscard]] const std::string& key() const { return key_; }

private:
    std::string key_;
};

void demo_shared_and_weak() {
    std::cout << "\n=== shared_ptr & weak_ptr Demo ===\n";

    std::weak_ptr<CacheEntry> observer;  // non-owning

    {
        // Two shared owners
        auto entry1 = std::make_shared<CacheEntry>("block-0x1A3F");
        auto entry2 = entry1;  // ref count = 2

        std::cout << "  ref count: " << entry1.use_count() << "\n";  // 2

        observer = entry1;  // weak_ptr does NOT increase ref count
        std::cout << "  ref count after weak: " << entry1.use_count() << "\n";  // still 2

        // To use a weak_ptr, you must lock() it to get a shared_ptr
        if (auto locked = observer.lock()) {
            std::cout << "  Observer sees: " << locked->key() << "\n";
        }

    }  // entry1 and entry2 destroyed → ref count = 0 → CacheEntry deleted

    // weak_ptr knows the object is gone
    std::cout << "  observer.expired() = " << std::boolalpha << observer.expired() << "\n";  // true
    if (auto locked = observer.lock()) {
        std::cout << "  This won't print\n";
    } else {
        std::cout << "  Object already destroyed — weak_ptr detected it\n";
    }
}

// ============================================================================
// Part 4: Ownership Patterns Summary
// ============================================================================

// FACTORY FUNCTION: Returns unique_ptr (caller owns the result)
std::unique_ptr<Connection> create_connection(const std::string& name) {
    return std::make_unique<Connection>(name);
}

// BORROWING: Takes raw pointer or reference (does NOT take ownership)
void use_connection(const Connection& conn) {
    // We borrow conn. We do NOT delete it. We do NOT store it beyond this call.
    std::cout << "  Using borrowed connection: " << conn.name() << "\n";
}

// SINK FUNCTION: Takes unique_ptr by value (takes ownership)
void take_connection(std::unique_ptr<Connection> conn) {
    conn->send("Final message before connection is consumed");
    // conn destroyed at end of scope — we took ownership
}

void demo_ownership_patterns() {
    std::cout << "\n=== Ownership Patterns ===\n";

    auto conn = create_connection("Worker-1");

    // Borrow — pass reference, caller keeps ownership
    use_connection(*conn);

    // Transfer — caller gives up ownership
    take_connection(std::move(conn));
    assert(conn == nullptr);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_unique_ptr();
    demo_custom_deleter();
    demo_shared_and_weak();
    demo_ownership_patterns();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. DEFAULT TO unique_ptr. Only use shared_ptr when ownership is truly shared.
//
// 2. NEVER use `new` directly. Use make_unique / make_shared.
//    Exception: custom deleters require the constructor form.
//
// 3. Function signatures communicate ownership intent:
//    - unique_ptr<T>        → "I create it, you own it" (factory)
//    - unique_ptr<T>&&      → "Give me ownership" (sink)  [or by value]
//    - const T& or T*       → "I borrow it, you still own it"
//    - shared_ptr<T>        → "We share ownership"
//
// 4. weak_ptr prevents CYCLES in shared_ptr graphs (e.g., parent↔child).
//
// 5. NEVER store a raw pointer to something owned by a smart pointer unless
//    you can guarantee the smart pointer outlives your usage.
//
// 6. shared_ptr has overhead: reference count (atomic increment/decrement),
//    separate control block allocation (unless make_shared is used).
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Create a `ResourcePool<T>` template that manages a pool of unique_ptr<T>.
//    Provide acquire() → unique_ptr<T> and release(unique_ptr<T>).
//
// 2. Wrap the POSIX `fopen`/`fclose` API in a unique_ptr with a custom deleter.
//    Write a function that reads a file and returns its contents as a string.
//
// 3. Implement a simple observer pattern where subjects hold shared_ptr to data
//    and observers hold weak_ptr. When data is updated, observers that are still
//    alive receive notifications.
//
// 4. DEBUGGING EXERCISE: The following code has a bug. Find it:
//      auto p = std::make_shared<int>(42);
//      int* raw = p.get();
//      p.reset();
//      std::cout << *raw << std::endl;  // What happens here?
// ============================================================================
