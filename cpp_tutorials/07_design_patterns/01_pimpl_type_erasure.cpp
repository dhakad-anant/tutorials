/**
 * Module 07 — Lesson 1: PIMPL, Type Erasure & Visitor
 *
 * These are C++-specific patterns that solve real problems in large codebases:
 * compilation firewalls, runtime polymorphism without inheritance, and
 * processing heterogeneous data structures.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o patterns1 01_pimpl_type_erasure.cpp
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <functional>

// ============================================================================
// Part 1: PIMPL (Pointer to Implementation) — Compilation Firewall
// ============================================================================

/*
 * PROBLEM: Changing a private member of a class forces ALL files that include
 *   its header to recompile. In a large codebase, this can mean rebuilding
 *   thousands of files.
 *
 * SOLUTION: Hide implementation details behind a pointer.
 *   - Header only declares a forward-declared Impl struct and a unique_ptr to it
 *   - Implementation is in the .cpp file
 *   - Changing internals only recompiles the .cpp, not all includers
 *
 * USED IN: Qt, Boost, many large C++ projects
 */

// --- What the HEADER would look like (widget.h) ---
class Widget {
public:
    Widget(std::string name, int value);
    ~Widget();  // Must be declared (destructor needs complete type)

    // Move operations (unique_ptr is move-only)
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;

    // Public interface
    void display() const;
    void update(int new_value);

private:
    struct Impl;  // Forward declaration — opaque to anyone including the header
    std::unique_ptr<Impl> pimpl_;
};

// --- What the .CPP would look like (widget.cpp) ---
struct Widget::Impl {
    std::string name;
    int value;
    std::vector<std::string> history;  // Adding this doesn't recompile includers!

    Impl(std::string n, int v) : name(std::move(n)), value(v) {}
};

Widget::Widget(std::string name, int value)
    : pimpl_(std::make_unique<Impl>(std::move(name), value)) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::display() const {
    std::cout << "  Widget[" << pimpl_->name << "] = " << pimpl_->value << "\n";
}

void Widget::update(int new_value) {
    pimpl_->history.push_back(std::to_string(pimpl_->value));
    pimpl_->value = new_value;
}

// ============================================================================
// Part 2: Type Erasure — Polymorphism Without Inheritance
// ============================================================================

/*
 * PROBLEM: std::function<void()> can hold ANY callable — lambda, function pointer,
 *   functor. How? It uses TYPE ERASURE internally.
 *
 * Type erasure = virtual dispatch + templates to store any type that satisfies
 * an interface, WITHOUT the stored type needing to inherit from anything.
 *
 * Think: std::function, std::any, std::unique_ptr with custom deleter
 */

// Let's build a type-erased "Printable" that can hold any type with a print() method

class Printable {
public:
    // Accept any type with a print() method
    template <typename T>
    Printable(T obj) : pimpl_(std::make_unique<Model<T>>(std::move(obj))) {}

    void print() const { pimpl_->print(); }

    // Copyable (deep copy)
    Printable(const Printable& other) : pimpl_(other.pimpl_->clone()) {}
    Printable& operator=(const Printable& other) {
        pimpl_ = other.pimpl_->clone();
        return *this;
    }
    Printable(Printable&&) noexcept = default;
    Printable& operator=(Printable&&) noexcept = default;

private:
    // Abstract base class (the "concept" — not C++20 concepts, the GoF pattern)
    struct Concept {
        virtual ~Concept() = default;
        virtual void print() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    // Concrete wrapper for any type T (the "model")
    template <typename T>
    struct Model : Concept {
        T data;
        explicit Model(T d) : data(std::move(d)) {}
        void print() const override { data.print(); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model<T>>(data);
        }
    };

    std::unique_ptr<Concept> pimpl_;
};

// These classes don't inherit from anything — they just have a print() method
struct Circle {
    double radius;
    void print() const { std::cout << "  Circle(r=" << radius << ")\n"; }
};

struct Square {
    double side;
    void print() const { std::cout << "  Square(s=" << side << ")\n"; }
};

struct Label {
    std::string text;
    void print() const { std::cout << "  Label(\"" << text << "\")\n"; }
};

void demo_type_erasure() {
    std::cout << "\n=== Type Erasure ===\n";

    // A heterogeneous collection — no common base class needed!
    std::vector<Printable> shapes;
    shapes.emplace_back(Circle{5.0});
    shapes.emplace_back(Square{3.0});
    shapes.emplace_back(Label{"hello"});

    for (const auto& s : shapes) {
        s.print();
    }
}

// ============================================================================
// Part 3: Visitor Pattern (using std::variant)
// ============================================================================

// Modern C++ visitor uses std::variant instead of virtual hierarchy

// Define AST nodes as simple structs
struct NumberExpr { double value; };
struct StringExpr { std::string value; };
struct BinaryExpr {
    char op;
    std::unique_ptr<std::variant<NumberExpr, StringExpr, BinaryExpr>> left;
    std::unique_ptr<std::variant<NumberExpr, StringExpr, BinaryExpr>> right;
};

// Simpler example: configuration values
using ConfigValue = std::variant<int, double, bool, std::string>;

// Overloaded helper for std::visit
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

std::string config_to_string(const ConfigValue& v) {
    return std::visit(overloaded{
        [](int i)                { return std::to_string(i); },
        [](double d)             { return std::to_string(d); },
        [](bool b)               { return std::string(b ? "true" : "false"); },
        [](const std::string& s) { return "\"" + s + "\""; },
    }, v);
}

size_t config_size_bytes(const ConfigValue& v) {
    return std::visit(overloaded{
        [](int)                { return sizeof(int); },
        [](double)             { return sizeof(double); },
        [](bool)               { return sizeof(bool); },
        [](const std::string& s) { return s.size(); },
    }, v);
}

void demo_visitor() {
    std::cout << "\n=== Visitor Pattern (std::variant) ===\n";

    std::vector<std::pair<std::string, ConfigValue>> config = {
        {"block_size",   4096},
        {"compression",  true},
        {"ratio",        0.85},
        {"device",       std::string("/dev/nvme0n1")},
    };

    for (const auto& [key, value] : config) {
        std::cout << "  " << key << " = " << config_to_string(value)
                  << " (" << config_size_bytes(value) << " bytes)\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== PIMPL Pattern ===\n";
    Widget w("counter", 42);
    w.display();
    w.update(100);
    w.display();

    // Move works
    Widget w2 = std::move(w);
    w2.display();

    demo_type_erasure();
    demo_visitor();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. PIMPL: compilation firewall. Use for stable ABIs and fast incremental builds.
//    Cost: one heap allocation, one pointer indirection per method.
//
// 2. Type Erasure: runtime polymorphism without inheritance. Used when you can't
//    or don't want to modify the stored types. Pattern: Concept (virtual base) +
//    Model<T> (templated wrapper).
//
// 3. Visitor (std::variant + std::visit + overloaded{}): process closed sets of
//    types. Compiler ensures you handle ALL alternatives. Preferred over virtual
//    dispatch when the type set is fixed.
//
// 4. In production: PIMPL for library interfaces, type erasure for callbacks/plugins,
//    variant visitor for protocol messages and config values.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Add a size() method to the type-erased Printable. It should return the
//    "size" of the underlying object (define what that means for each shape).
//
// 2. Build a type-erased `Any` class (simplified std::any) that supports
//    any_cast<T> for safe retrieval.
//
// 3. Implement a command processor using variant visitor:
//    Command = variant<ReadCmd, WriteCmd, DeleteCmd, FlushCmd>
//    Each command has different fields. Use visit to dispatch to handlers.
//
// 4. Write a PIMPL-based `Logger` class. The header should expose only
//    log(level, message). The impl should hold file handles, formatting state,
//    and buffering — all hidden from the header.
// ============================================================================
