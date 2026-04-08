/**
 * Module 05 — Lesson 2: Serialization & Binary Protocols
 *
 * WHY THIS MATTERS:
 *   Storage systems communicate via binary protocols — between nodes, between
 *   firmware and host, between disk and controller. Understanding how to
 *   serialize/deserialize data correctly (endianness, alignment, versioning)
 *   is essential.
 *
 * COMPILE: g++ -std=c++20 -Wall -Wextra -Werror -o serialization 02_serialization.cpp
 */

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <bit>          // std::endian (C++20)
#include <algorithm>
#include <array>

// ============================================================================
// Part 1: Endianness — Byte Order Matters
// ============================================================================

void demo_endianness() {
    std::cout << "=== Endianness ===\n";

    // Detect system endianness (C++20)
    if constexpr (std::endian::native == std::endian::little) {
        std::cout << "  System is LITTLE-ENDIAN (x86, ARM default)\n";
    } else {
        std::cout << "  System is BIG-ENDIAN (network byte order)\n";
    }

    // Demonstrate byte layout
    uint32_t value = 0x01020304;
    auto* bytes = reinterpret_cast<uint8_t*>(&value);
    std::cout << "  0x01020304 in memory: ";
    for (int i = 0; i < 4; ++i) {
        std::cout << std::hex << static_cast<int>(bytes[i]) << " ";
    }
    std::cout << std::dec << "\n";
    // Little-endian: 04 03 02 01
    // Big-endian:    01 02 03 04
}

// Portable byte-swap functions
inline uint16_t swap16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

inline uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0xFF)       |
           ((v >> 8)  & 0xFF00)     |
           ((v << 8)  & 0xFF0000)   |
           ((v << 24) & 0xFF000000);
}

// Convert to/from network byte order (big-endian)
inline uint32_t to_network(uint32_t v) {
    if constexpr (std::endian::native == std::endian::little) {
        return swap32(v);
    }
    return v;
}

inline uint32_t from_network(uint32_t v) {
    return to_network(v);  // swap is its own inverse
}

// ============================================================================
// Part 2: Binary Serialization Buffer
// ============================================================================

class BinaryWriter {
public:
    // Write raw bytes
    void write_bytes(const void* data, size_t len) {
        const auto* src = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), src, src + len);
    }

    // Write integer (network byte order)
    void write_u8(uint8_t v)   { buffer_.push_back(v); }
    void write_u16(uint16_t v) { v = to_network(static_cast<uint32_t>(v)); write_bytes(&v, 2); }
    void write_u32(uint32_t v) { v = to_network(v); write_bytes(&v, 4); }
    void write_u64(uint64_t v) {
        write_u32(static_cast<uint32_t>(v >> 32));
        write_u32(static_cast<uint32_t>(v));
    }

    // Write length-prefixed string
    void write_string(const std::string& s) {
        write_u32(static_cast<uint32_t>(s.size()));
        write_bytes(s.data(), s.size());
    }

    [[nodiscard]] const std::vector<uint8_t>& data() const { return buffer_; }
    [[nodiscard]] size_t size() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

class BinaryReader {
public:
    explicit BinaryReader(const uint8_t* data, size_t len)
        : data_(data), len_(len), pos_(0) {}

    explicit BinaryReader(const std::vector<uint8_t>& buf)
        : data_(buf.data()), len_(buf.size()), pos_(0) {}

    uint8_t read_u8() {
        check(1);
        return data_[pos_++];
    }

    uint16_t read_u16() {
        check(2);
        uint16_t v;
        std::memcpy(&v, data_ + pos_, 2);
        pos_ += 2;
        return static_cast<uint16_t>(from_network(v));
    }

    uint32_t read_u32() {
        check(4);
        uint32_t v;
        std::memcpy(&v, data_ + pos_, 4);
        pos_ += 4;
        return from_network(v);
    }

    uint64_t read_u64() {
        uint64_t hi = read_u32();
        uint64_t lo = read_u32();
        return (hi << 32) | lo;
    }

    std::string read_string() {
        uint32_t len = read_u32();
        check(len);
        std::string s(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return s;
    }

    [[nodiscard]] size_t remaining() const { return len_ - pos_; }

private:
    void check(size_t n) const {
        if (pos_ + n > len_) {
            throw std::runtime_error("BinaryReader: read beyond end of buffer");
        }
    }

    const uint8_t* data_;
    size_t len_;
    size_t pos_;
};

// ============================================================================
// Part 3: Protocol Message Example
// ============================================================================

// A storage protocol message:
// [1 byte: type] [4 bytes: request_id] [8 bytes: offset] [4 bytes: length]
// [4 bytes: data_len] [N bytes: data]

enum class MessageType : uint8_t {
    READ_REQUEST  = 0x01,
    WRITE_REQUEST = 0x02,
    READ_RESPONSE = 0x81,
    WRITE_RESPONSE = 0x82,
};

struct StorageMessage {
    MessageType type;
    uint32_t    request_id;
    uint64_t    offset;
    uint32_t    length;
    std::string data;

    void serialize(BinaryWriter& writer) const {
        writer.write_u8(static_cast<uint8_t>(type));
        writer.write_u32(request_id);
        writer.write_u64(offset);
        writer.write_u32(length);
        writer.write_string(data);
    }

    static StorageMessage deserialize(BinaryReader& reader) {
        StorageMessage msg;
        msg.type = static_cast<MessageType>(reader.read_u8());
        msg.request_id = reader.read_u32();
        msg.offset = reader.read_u64();
        msg.length = reader.read_u32();
        msg.data = reader.read_string();
        return msg;
    }
};

void demo_protocol() {
    std::cout << "\n=== Binary Protocol ===\n";

    // Create a write request
    StorageMessage request{
        .type = MessageType::WRITE_REQUEST,
        .request_id = 42,
        .offset = 0x1000'0000ULL,
        .length = 4096,
        .data = "block data here..."
    };

    // Serialize
    BinaryWriter writer;
    request.serialize(writer);
    std::cout << "  Serialized: " << writer.size() << " bytes\n";

    // Hex dump first 32 bytes
    std::cout << "  Hex: ";
    for (size_t i = 0; i < std::min<size_t>(32, writer.size()); ++i) {
        printf("%02x ", writer.data()[i]);
    }
    std::cout << "...\n";

    // Deserialize
    BinaryReader reader(writer.data());
    auto decoded = StorageMessage::deserialize(reader);

    std::cout << "  Decoded: type=0x" << std::hex << static_cast<int>(decoded.type)
              << " req_id=" << std::dec << decoded.request_id
              << " offset=0x" << std::hex << decoded.offset
              << " len=" << std::dec << decoded.length
              << " data=\"" << decoded.data << "\"\n";

    assert(decoded.request_id == request.request_id);
    assert(decoded.offset == request.offset);
    assert(decoded.data == request.data);
    std::cout << "  ✓ Round-trip successful\n";
}

// ============================================================================
// Part 4: Zero-Copy Deserialization (View into Buffer)
// ============================================================================

// Instead of copying strings out of the buffer, return views into the buffer.
// This is the pattern used in high-performance parsers (flatbuffers, Cap'n Proto).

class ZeroCopyReader {
public:
    explicit ZeroCopyReader(const uint8_t* data, size_t len)
        : data_(data), len_(len), pos_(0) {}

    // Returns a VIEW into the buffer — no copy!
    std::string_view read_string_view() {
        uint32_t len = read_u32();
        if (pos_ + len > len_) throw std::runtime_error("overflow");
        std::string_view sv(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return sv;  // Only valid while the underlying buffer exists!
    }

    uint32_t read_u32() {
        if (pos_ + 4 > len_) throw std::runtime_error("overflow");
        uint32_t v;
        std::memcpy(&v, data_ + pos_, 4);
        pos_ += 4;
        return from_network(v);
    }

private:
    const uint8_t* data_;
    size_t len_;
    size_t pos_;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    demo_endianness();
    demo_protocol();

    std::cout << "\n=== Zero-Copy Reader ===\n";
    BinaryWriter w;
    w.write_string("hello zero-copy");
    ZeroCopyReader zcr(w.data().data(), w.size());
    auto sv = zcr.read_string_view();
    std::cout << "  Zero-copy view: \"" << sv << "\" (no allocation)\n";

    return 0;
}

// ============================================================================
// KEY TAKEAWAYS:
//
// 1. ALWAYS handle endianness in network/disk protocols. Use explicit conversion.
// 2. Use memcpy for type-punning, NOT reinterpret_cast (avoids UB/alignment issues).
// 3. Length-prefix strings and arrays. Never trust incoming size values — validate.
// 4. Zero-copy: return string_view / span into the buffer. Watch lifetimes!
// 5. Version your protocol. Add a version byte at the start of every message.
// 6. In production, use established formats: protobuf, flatbuffers, Cap'n Proto.
//    But understand the principles to debug and extend them.
// ============================================================================

// ============================================================================
// EXERCISES:
//
// 1. Add a CRC32 checksum to StorageMessage. Compute it over the serialized
//    content (excluding the checksum field itself). Verify on deserialization.
//
// 2. Add versioning: v1 has the current fields, v2 adds a `priority` field.
//    Write backward-compatible deserialization that handles both versions.
//
// 3. Implement a TLV (Type-Length-Value) encoder/decoder. Each field is:
//    [1 byte type] [4 bytes length] [N bytes value]. This is how many
//    network protocols (DHCP, TLS extensions) work.
//
// 4. Benchmark: serialize/deserialize 1M messages. Compare your hand-rolled
//    serializer vs nlohmann::json. Measure both speed and output size.
// ============================================================================
