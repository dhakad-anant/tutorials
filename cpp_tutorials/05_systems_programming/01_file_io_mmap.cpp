/**
 * Module 05 — Lesson 1: File I/O, mmap & Zero-Copy
 *
 * WHY THIS MATTERS:
 *   Pure Storage builds storage systems. Understanding how data moves between disk,
 *   kernel, and user space — and how to minimize copies — is fundamental.
 *
 * NOTE: Some system calls (mmap, pread) are POSIX-specific. On Windows, use
 *       CreateFileMapping/MapViewOfFile. The concepts are identical.
 *
 * COMPILE (Linux): g++ -std=c++20 -Wall -Wextra -Werror -o file_io 01_file_io_mmap.cpp
 * COMPILE (Windows): cl /std:c++20 /EHsc /W4 01_file_io_mmap.cpp
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <memory>
#include <filesystem>
#include <cassert>
#include <array>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

// ============================================================================
// Part 1: C++ fstream I/O (The Standard Way)
// ============================================================================

void demo_fstream() {
    std::cout << "=== fstream I/O ===\n";

    const std::string path = "test_fstream.bin";

    // Write binary data
    {
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            std::cerr << "  Failed to open for writing\n";
            return;
        }
        std::vector<int> data(1000);
        for (int i = 0; i < 1000; ++i) data[i] = i * i;

        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size() * sizeof(int)));
        std::cout << "  Wrote " << data.size() * sizeof(int) << " bytes\n";
    }

    // Read binary data
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "  Failed to open for reading\n";
            return;
        }
        std::vector<int> data(1000);
        in.read(reinterpret_cast<char*>(data.data()),
                static_cast<std::streamsize>(data.size() * sizeof(int)));

        std::cout << "  Read back: data[42] = " << data[42] << " (expected " << 42*42 << ")\n";
    }

    std::filesystem::remove(path);
}

// ============================================================================
// Part 2: Buffered vs Direct I/O Concepts
// ============================================================================

/*
 * BUFFERED I/O (default):
 *   App → C library buffer → Kernel page cache → Disk
 *   - Kernel caches aggressively. Good for repeated reads.
 *   - Bad for large sequential scans (pollutes page cache).
 *
 * DIRECT I/O (O_DIRECT on Linux):
 *   App → Disk (bypasses kernel page cache)
 *   - Buffer must be aligned (usually 512 or 4096 bytes).
 *   - Used by databases and storage systems that manage their own cache.
 *   - Pure Storage likely uses this for the data path.
 *
 * MEMORY-MAPPED I/O (mmap):
 *   Disk pages are mapped into the process address space.
 *   Reading from the mapping triggers a page fault → kernel loads from disk.
 *   - Zero-copy for reads (data goes directly into your address space).
 *   - Great for random access patterns (like B-trees, indexes).
 *   - The kernel manages caching via the page cache.
 */

// ============================================================================
// Part 3: mmap — Memory-Mapped Files
// ============================================================================

#ifndef _WIN32
// POSIX version
void demo_mmap_posix() {
    std::cout << "\n=== mmap (POSIX) ===\n";

    const char* path = "test_mmap.bin";
    constexpr size_t FILE_SIZE = 4096;

    // Create and write a file
    {
        int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
        if (fd < 0) { perror("open"); return; }
        ftruncate(fd, FILE_SIZE);

        // mmap for writing
        void* addr = mmap(nullptr, FILE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) { perror("mmap"); close(fd); return; }

        // Write directly to the mapping (no read/write syscalls!)
        auto* data = static_cast<int*>(addr);
        for (int i = 0; i < 1024; ++i) {
            data[i] = i;
        }

        // Flush to disk
        msync(addr, FILE_SIZE, MS_SYNC);
        munmap(addr, FILE_SIZE);
        close(fd);
        std::cout << "  Wrote 1024 ints via mmap\n";
    }

    // Read back via mmap
    {
        int fd = open(path, O_RDONLY);
        void* addr = mmap(nullptr, FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
        auto* data = static_cast<const int*>(addr);

        std::cout << "  data[42] = " << data[42] << " (expected 42)\n";
        std::cout << "  data[100] = " << data[100] << " (expected 100)\n";

        munmap(const_cast<void*>(static_cast<const void*>(addr)), FILE_SIZE);
        close(fd);
    }

    unlink(path);
}
#endif

// ============================================================================
// Part 4: RAII Wrapper for mmap
// ============================================================================

#ifndef _WIN32
class MappedRegion {
public:
    static std::unique_ptr<MappedRegion> open(const std::string& path) {
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) return nullptr;

        struct stat st;
        if (fstat(fd, &st) < 0) { ::close(fd); return nullptr; }

        void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd);  // fd can be closed after mmap
        if (addr == MAP_FAILED) return nullptr;

        return std::unique_ptr<MappedRegion>(
            new MappedRegion(addr, static_cast<size_t>(st.st_size)));
    }

    ~MappedRegion() {
        if (addr_) munmap(addr_, size_);
    }

    MappedRegion(const MappedRegion&) = delete;
    MappedRegion& operator=(const MappedRegion&) = delete;
    MappedRegion(MappedRegion&& other) noexcept
        : addr_(std::exchange(other.addr_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}

    [[nodiscard]] const void* data() const { return addr_; }
    [[nodiscard]] size_t size() const { return size_; }

    template <typename T>
    [[nodiscard]] const T* as() const { return static_cast<const T*>(addr_); }

private:
    MappedRegion(void* addr, size_t size) : addr_(addr), size_(size) {}
    void* addr_;
    size_t size_;
};
#endif

// ============================================================================
// Part 5: Scatter/Gather I/O Concept
// ============================================================================

/*
 * In high-performance I/O, instead of reading into a single buffer, you can
 * read into MULTIPLE buffers in one syscall:
 *
 * readv(fd, iovec[], count)  — scatter: read from file into multiple buffers
 * writev(fd, iovec[], count) — gather: write multiple buffers to file
 *
 * Why? Fewer syscalls, fewer context switches. The kernel can optimize the DMA
 * transfers. Essential for network protocols and storage systems.
 *
 * struct iovec {
 *     void*  iov_base;   // buffer address
 *     size_t iov_len;    // buffer length
 * };
 */

// ============================================================================
// Part 6: Benchmark — read() vs mmap for sequential access
// ============================================================================

void demo_benchmark_io() {
    std::cout << "\n=== I/O Benchmark ===\n";

    constexpr size_t SIZE = 10 * 1024 * 1024;  // 10 MB
    const std::string path = "bench_io.bin";

    // Create test file
    {
        std::ofstream out(path, std::ios::binary);
        std::vector<char> data(SIZE, 'A');
        out.write(data.data(), static_cast<std::streamsize>(SIZE));
    }

    // Benchmark fstream read
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::ifstream in(path, std::ios::binary);
        std::vector<char> buf(SIZE);
        in.read(buf.data(), static_cast<std::streamsize>(SIZE));
        long long sum = 0;
        for (char c : buf) sum += c;
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  fstream:  " << ms.count() << " ms (sum=" << sum << ")\n";
    }

#ifndef _WIN32
    // Benchmark mmap
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto region = MappedRegion::open(path);
        long long sum = 0;
        auto* data = region->as<char>();
        for (size_t i = 0; i < region->size(); ++i) sum += data[i];
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "  mmap:     " << ms.count() << " ms (sum=" << sum << ")\n";
    }
#endif

    std::filesystem::remove(path);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_fstream();

#ifndef _WIN32
    demo_mmap_posix();
#else
    std::cout << "\n  (mmap demo skipped on Windows — concept is the same with"
              << " CreateFileMapping/MapViewOfFile)\n";
#endif

    demo_benchmark_io();

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. fstream: portable, buffered, fine for most use cases.
// 2. mmap: zero-copy, great for random access and large files. RAII-wrap it.
// 3. O_DIRECT: bypasses page cache. Used when you manage your own cache.
// 4. Scatter/gather (readv/writev): fewer syscalls for multi-buffer I/O.
// 5. Always close/unmap resources (RAII). A leaked mmap is a memory leak.
// 6. For storage systems: understand read-ahead, page cache, and DMA paths.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Write a RAII MappedFile class for Windows using CreateFileMapping and
//    MapViewOfFile. Provide the same API as the POSIX MappedRegion above.
//
// 2. Implement a simple log-structured file: append-only writes, read by offset.
//    Use mmap for the index and direct write for the data segment.
//
// 3. Benchmark: read a 1GB file sequentially with different buffer sizes
//    (4KB, 64KB, 1MB, 4MB). Plot throughput. What's the optimal read size?
//
// 4. Write a scatter/gather network sender: given a protocol header + body +
//    checksum, send all three in one writev() call.
// ============================================================================
