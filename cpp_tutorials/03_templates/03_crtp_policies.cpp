/**
 * Module 03 — Lesson 3: CRTP & Policy-Based Design
 *
 * WHY THIS MATTERS:
 *   CRTP (Curiously Recurring Template Pattern) gives you static polymorphism —
 *   the flexibility of virtual functions WITHOUT the runtime cost. Policy-based
 *   design lets you compose behavior from building blocks at compile time.
 *   Both are heavily used in performance-critical infrastructure code.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o crtp 03_crtp_policies.cpp
 */

#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cstring>

// ============================================================================
// Part 1: CRTP Basics — Static Polymorphism
// ============================================================================

// CRTP: Base class is templated on derived class
// Derived passes ITSELF as the template argument to Base

template <typename Derived>
class Countable {
public:
    static int count() { return count_; }

protected:
    Countable()  { ++count_; }
    ~Countable() { --count_; }
    Countable(const Countable&) { ++count_; }

private:
    static inline int count_ = 0;
};

class Widget : public Countable<Widget> {
public:
    explicit Widget(std::string name) : name_(std::move(name)) {}
    [[nodiscard]] const std::string& name() const { return name_; }
private:
    std::string name_;
};

class Gadget : public Countable<Gadget> {
public:
    explicit Gadget(int id) : id_(id) {}
private:
    int id_;
};

// Each class gets its OWN counter — no virtual dispatch needed

// ============================================================================
// Part 2: CRTP for Static Interface / Mixin
// ============================================================================

// Add functionality to any derived class without virtual dispatch

template <typename Derived>
class Serializable {
public:
    std::string to_json() const {
        // Calls the derived class's fields() method — resolved at compile time
        return static_cast<const Derived*>(this)->serialize_impl();
    }

    void save(const std::string& path) const {
        auto json = to_json();
        std::cout << "  Saving to " << path << ": " << json << "\n";
    }
};

class Config : public Serializable<Config> {
public:
    Config(std::string name, int value) : name_(std::move(name)), value_(value) {}

    // CRTP requires this method to exist — compile error if missing
    std::string serialize_impl() const {
        return "{\"name\":\"" + name_ + "\",\"value\":" + std::to_string(value_) + "}";
    }

private:
    std::string name_;
    int value_;
};

// ============================================================================
// Part 3: CRTP for Operator Generation
// ============================================================================

// Implement < only, get ==, !=, <=, >, >= for free (pre-C++20 pattern)
// (In C++20, use <=> spaceship operator instead)

template <typename Derived>
class Comparable {
public:
    friend bool operator==(const Derived& a, const Derived& b) {
        return !(a < b) && !(b < a);
    }
    friend bool operator!=(const Derived& a, const Derived& b) {
        return !(a == b);
    }
    friend bool operator>(const Derived& a, const Derived& b) {
        return b < a;
    }
    friend bool operator<=(const Derived& a, const Derived& b) {
        return !(b < a);
    }
    friend bool operator>=(const Derived& a, const Derived& b) {
        return !(a < b);
    }
};

class Version : public Comparable<Version> {
public:
    Version(int major, int minor) : major_(major), minor_(minor) {}

    friend bool operator<(const Version& a, const Version& b) {
        if (a.major_ != b.major_) return a.major_ < b.major_;
        return a.minor_ < b.minor_;
    }

    friend std::ostream& operator<<(std::ostream& os, const Version& v) {
        return os << v.major_ << "." << v.minor_;
    }

private:
    int major_, minor_;
};

// ============================================================================
// Part 4: Policy-Based Design
// ============================================================================

// Compose behavior by mixing in "policy" classes via templates
// Each policy handles one aspect of behavior

// --- Locking Policies ---
struct NoLock {
    void lock() {}
    void unlock() {}
};

struct MutexLock {
    void lock()   { std::cout << "    [mutex lock]\n"; }
    void unlock() { std::cout << "    [mutex unlock]\n"; }
};

// --- Logging Policies ---
struct NoLog {
    void log([[maybe_unused]] const std::string& msg) {}
};

struct ConsoleLog {
    void log(const std::string& msg) { std::cout << "    [LOG] " << msg << "\n"; }
};

// --- The class composed from policies ---
template <typename LockPolicy = NoLock, typename LogPolicy = NoLog>
class Cache : private LockPolicy, private LogPolicy {
public:
    void put(const std::string& key, int value) {
        LockPolicy::lock();
        LogPolicy::log("put(" + key + ", " + std::to_string(value) + ")");
        // ... actual cache logic ...
        std::cout << "  Cache: stored " << key << "=" << value << "\n";
        LockPolicy::unlock();
    }

    int get(const std::string& key) {
        LockPolicy::lock();
        LogPolicy::log("get(" + key + ")");
        LockPolicy::unlock();
        return 42;  // simulated
    }
};

// The caller chooses exactly what behavior they need:
// Cache<>                        — no locking, no logging (fastest)
// Cache<MutexLock>               — thread-safe, no logging
// Cache<MutexLock, ConsoleLog>   — thread-safe with logging (debug)

// ============================================================================
// Part 5: CRTP vs Virtual — Performance Comparison
// ============================================================================

// Virtual dispatch (traditional polymorphism)
class ShapeVirtual {
public:
    virtual ~ShapeVirtual() = default;
    virtual double area() const = 0;
};

class CircleVirtual : public ShapeVirtual {
    double r_;
public:
    explicit CircleVirtual(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }
};

// CRTP dispatch (static polymorphism)
template <typename Derived>
class ShapeCRTP {
public:
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

class CircleCRTP : public ShapeCRTP<CircleCRTP> {
    double r_;
public:
    explicit CircleCRTP(double r) : r_(r) {}
    double area_impl() const { return 3.14159265 * r_ * r_; }
};

void benchmark_dispatch() {
    std::cout << "\n=== CRTP vs Virtual Dispatch ===\n";
    constexpr int N = 10'000'000;

    // Virtual
    {
        CircleVirtual c(5.0);
        ShapeVirtual* ptr = &c;
        auto start = std::chrono::high_resolution_clock::now();
        double sum = 0;
        for (int i = 0; i < N; ++i) sum += ptr->area();
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  Virtual: " << us.count() << " µs (sum=" << sum << ")\n";
    }

    // CRTP
    {
        CircleCRTP c(5.0);
        auto start = std::chrono::high_resolution_clock::now();
        double sum = 0;
        for (int i = 0; i < N; ++i) sum += c.area();
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  CRTP:    " << us.count() << " µs (sum=" << sum << ")\n";
    }

    // CRTP wins because:
    // 1. No vtable pointer dereference
    // 2. Compiler can inline area_impl() directly
    // 3. Better branch prediction (no indirect call)
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== CRTP: Instance Counting ===\n";
    {
        Widget w1("A"), w2("B");
        Gadget g1(1);
        std::cout << "  Widgets: " << Widget::count() << "\n";  // 2
        std::cout << "  Gadgets: " << Gadget::count() << "\n";   // 1
    }
    std::cout << "  After scope: Widgets=" << Widget::count()
              << " Gadgets=" << Gadget::count() << "\n";  // 0, 0

    std::cout << "\n=== CRTP: Serialization Mixin ===\n";
    Config cfg("block_size", 4096);
    cfg.save("/etc/storage.json");

    std::cout << "\n=== CRTP: Operator Generation ===\n";
    Version v1(2, 1), v2(2, 3), v3(2, 1);
    std::cout << "  " << v1 << " < " << v2 << ": " << std::boolalpha << (v1 < v2) << "\n";
    std::cout << "  " << v1 << " == " << v3 << ": " << (v1 == v3) << "\n";
    std::cout << "  " << v2 << " > " << v1 << ": " << (v2 > v1) << "\n";

    std::cout << "\n=== Policy-Based Design ===\n";
    std::cout << "  --- No lock, no log ---\n";
    Cache<> fast_cache;
    fast_cache.put("key1", 100);

    std::cout << "  --- Mutex + Console Log ---\n";
    Cache<MutexLock, ConsoleLog> debug_cache;
    debug_cache.put("key2", 200);

    benchmark_dispatch();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. CRTP = Derived inherits from Base<Derived>. Gives static polymorphism.
// 2. Use CRTP for: mixins, operator generation, compile-time interface enforcement.
// 3. CRTP advantage over virtual: zero overhead, inlinable, better optimization.
// 4. CRTP disadvantage: no runtime polymorphism (can't store Base<?>* in a vector).
// 5. Policy-based design = compose behavior via template parameters.
//    Each policy is a small, single-responsibility class.
// 6. In C++20, many CRTP use cases are better served by concepts + deducing this.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a CRTP mixin `Cloneable<Derived>` that provides a clone() method
//    returning unique_ptr<Derived>. Test with a complex class.
//
// 2. Design a Logger with policies for: output destination (Console, File, Null),
//    formatting (Plain, JSON), and filtering (All, WarningsOnly).
//
// 3. Implement a CRTP `Singleton<Derived>` that provides a static instance()
//    method. Discuss: why is Singleton generally an anti-pattern?
//
// 4. Rewrite the Comparable CRTP to use C++20's <=> operator. Which is simpler?
// ============================================================================
