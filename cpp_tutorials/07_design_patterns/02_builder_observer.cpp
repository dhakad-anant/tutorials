/**
 * Module 07 — Lesson 2: Builder, Observer & Command Patterns
 *
 * Practical patterns used extensively in systems code.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o patterns2 02_builder_observer.cpp
 */

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <queue>
#include <variant>

// ============================================================================
// Part 1: Builder Pattern — Construct Complex Objects Step by Step
// ============================================================================

struct StorageConfig {
    std::string device_path = "/dev/sda";
    size_t      block_size  = 4096;
    size_t      cache_size_mb = 256;
    bool        compression = false;
    bool        encryption  = false;
    int         io_threads  = 4;
    std::string log_level   = "info";

    void print() const {
        std::cout << "  Config: device=" << device_path
                  << " block=" << block_size
                  << " cache=" << cache_size_mb << "MB"
                  << " compression=" << std::boolalpha << compression
                  << " encryption=" << encryption
                  << " threads=" << io_threads
                  << " log=" << log_level << "\n";
    }
};

class StorageConfigBuilder {
public:
    StorageConfigBuilder& device(std::string path) {
        config_.device_path = std::move(path);
        return *this;
    }
    StorageConfigBuilder& block_size(size_t bs) {
        config_.block_size = bs;
        return *this;
    }
    StorageConfigBuilder& cache_mb(size_t mb) {
        config_.cache_size_mb = mb;
        return *this;
    }
    StorageConfigBuilder& enable_compression(bool v = true) {
        config_.compression = v;
        return *this;
    }
    StorageConfigBuilder& enable_encryption(bool v = true) {
        config_.encryption = v;
        return *this;
    }
    StorageConfigBuilder& io_threads(int n) {
        config_.io_threads = n;
        return *this;
    }
    StorageConfigBuilder& log_level(std::string level) {
        config_.log_level = std::move(level);
        return *this;
    }

    // Validate and build
    StorageConfig build() {
        if (config_.block_size == 0 || (config_.block_size & (config_.block_size - 1)) != 0) {
            throw std::invalid_argument("block_size must be a power of 2");
        }
        if (config_.io_threads <= 0) {
            throw std::invalid_argument("io_threads must be positive");
        }
        return config_;
    }

private:
    StorageConfig config_;
};

void demo_builder() {
    std::cout << "=== Builder Pattern ===\n";

    auto config = StorageConfigBuilder()
        .device("/dev/nvme0n1")
        .block_size(8192)
        .cache_mb(1024)
        .enable_compression()
        .enable_encryption()
        .io_threads(8)
        .log_level("debug")
        .build();

    config.print();

    // Default config
    auto default_config = StorageConfigBuilder().build();
    default_config.print();
}

// ============================================================================
// Part 2: Observer Pattern — Event System
// ============================================================================

// Modern C++ observer using std::function (no base class inheritance needed)

template <typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;
    using SlotId = size_t;

    SlotId connect(SlotType slot) {
        SlotId id = next_id_++;
        slots_.emplace_back(id, std::move(slot));
        return id;
    }

    void disconnect(SlotId id) {
        slots_.erase(
            std::remove_if(slots_.begin(), slots_.end(),
                           [id](const auto& p) { return p.first == id; }),
            slots_.end());
    }

    void emit(Args... args) const {
        for (const auto& [id, slot] : slots_) {
            slot(args...);
        }
    }

private:
    std::vector<std::pair<SlotId, SlotType>> slots_;
    SlotId next_id_ = 0;
};

// Usage: a disk monitor that emits events
class DiskMonitor {
public:
    Signal<std::string, double>& on_threshold_exceeded() { return threshold_signal_; }
    Signal<std::string>&         on_error() { return error_signal_; }

    void check_disk(const std::string& device, double usage_pct) {
        if (usage_pct > 90.0) {
            threshold_signal_.emit(device, usage_pct);
        }
        if (usage_pct > 99.0) {
            error_signal_.emit(device + ": CRITICAL — disk almost full!");
        }
    }

private:
    Signal<std::string, double> threshold_signal_;
    Signal<std::string>         error_signal_;
};

void demo_observer() {
    std::cout << "\n=== Observer Pattern ===\n";

    DiskMonitor monitor;

    // Connect observers (no inheritance!)
    monitor.on_threshold_exceeded().connect(
        [](const std::string& dev, double pct) {
            std::cout << "  [WARNING] " << dev << " at " << pct << "% usage\n";
        });

    auto error_id = monitor.on_error().connect(
        [](const std::string& msg) {
            std::cout << "  [ERROR] " << msg << "\n";
        });

    monitor.check_disk("/dev/sda1", 85);   // below threshold
    monitor.check_disk("/dev/sda1", 95);   // warning
    monitor.check_disk("/dev/nvme0", 99.5); // warning + error

    // Disconnect error handler
    monitor.on_error().disconnect(error_id);
    monitor.check_disk("/dev/sdb1", 99.9);  // warning only (error disconnected)
}

// ============================================================================
// Part 3: Command Pattern — Encapsulate Operations
// ============================================================================

// Commands as value types (using variant for closed set of operations)

struct ReadCommand {
    uint64_t block_id;
    size_t   length;
};

struct WriteCommand {
    uint64_t    block_id;
    std::string data;
};

struct FlushCommand {};

struct TrimCommand {
    uint64_t start_block;
    uint64_t end_block;
};

using Command = std::variant<ReadCommand, WriteCommand, FlushCommand, TrimCommand>;

class CommandProcessor {
public:
    void enqueue(Command cmd) {
        queue_.push(std::move(cmd));
    }

    void process_all() {
        while (!queue_.empty()) {
            auto cmd = std::move(queue_.front());
            queue_.pop();
            process(cmd);
        }
    }

private:
    void process(const Command& cmd) {
        std::visit(overloaded{
            [](const ReadCommand& c) {
                std::cout << "  READ block=" << c.block_id
                          << " len=" << c.length << "\n";
            },
            [](const WriteCommand& c) {
                std::cout << "  WRITE block=" << c.block_id
                          << " data_len=" << c.data.size() << "\n";
            },
            [](const FlushCommand&) {
                std::cout << "  FLUSH all pending writes\n";
            },
            [](const TrimCommand& c) {
                std::cout << "  TRIM blocks [" << c.start_block
                          << ", " << c.end_block << ")\n";
            },
        }, cmd);
    }

    // Reuse the overloaded helper from Module 07, Lesson 1
    template <class... Ts>
    struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    std::queue<Command> queue_;
};

void demo_command() {
    std::cout << "\n=== Command Pattern ===\n";

    CommandProcessor proc;
    proc.enqueue(WriteCommand{0x100, "hello world"});
    proc.enqueue(WriteCommand{0x101, "more data"});
    proc.enqueue(ReadCommand{0x100, 4096});
    proc.enqueue(FlushCommand{});
    proc.enqueue(TrimCommand{0x200, 0x300});
    proc.process_all();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_builder();
    demo_observer();
    demo_command();
    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. Builder: chain method calls to construct complex objects. Validate in build().
// 2. Observer (Signal/Slot): decouple event producers from consumers.
//    No inheritance needed — use std::function.
// 3. Command (variant-based): represent operations as data. Queue, log, replay,
//    undo — all easy when operations are values.
// 4. These patterns are about DECOUPLING: separating construction from use,
//    separating event production from consumption, separating command creation
//    from execution.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Add undo support to CommandProcessor: each command returns an "undo" command.
//    WriteCommand's undo is the previous data. Implement an undo() method.
//
// 2. Extend Signal<> to support priorities: high-priority slots fire first.
//
// 3. Write a ConnectionBuilder that creates a network connection with options:
//    host, port, timeout, TLS, retry_count, etc. Validate all options in build().
//
// 4. Combine Builder + Command: a QueryBuilder that builds ReadCommand/WriteCommand
//    objects from a fluent interface:
//      auto cmd = QueryBuilder().read().block(0x100).length(4096).build();
// ============================================================================
