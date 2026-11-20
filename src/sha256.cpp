// language: C++, file: sha256.cpp, target: Windows/Linux, C++20
// *FIPS 180-4 SHA-256. sha256d = SHA-256(SHA-256(x)), Bitcoin-style.*
#include "sha256.hpp"

#include <cstring>
#include <optional>

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
inline void store_be32(uint8_t* p, uint32_t x) {
    p[0] = uint8_t(x >> 24); p[1] = uint8_t(x >> 16); p[2] = uint8_t(x >> 8); p[3] = uint8_t(x);
}
inline void store_be64(uint8_t* p, uint64_t x) {
    for (int i = 0; i < 8; ++i) p[i] = uint8_t(x >> (56 - 8 * i));
}

void compress(uint32_t s[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = be32(block + 4 * i);
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4], f = s[5], g = s[6], h = s[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s[0] += a; s[1] += b; s[2] += c; s[3] += d;
    s[4] += e; s[5] += f; s[6] += g; s[7] += h;
}

}  // namespace

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t n) {
    uint32_t s[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t block[64];
    size_t i = 0;
    for (; i + 64 <= n; i += 64) compress(s, data + i);
    size_t rem = n - i;
    std::memset(block, 0, 64);
    if (rem) std::memcpy(block, data + i, rem);
    block[rem] = 0x80;
    uint64_t bits = uint64_t(n) * 8;
    if (rem >= 56) {
        compress(s, block);
        std::memset(block, 0, 64);
    }
    store_be64(block + 56, bits);
    compress(s, block);
    std::array<uint8_t, 32> out{};
    for (int k = 0; k < 8; ++k) store_be32(out.data() + 4 * k, s[k]);
    return out;
}

std::array<uint8_t, 32> sha256(const std::vector<uint8_t>& data) {
    return sha256(data.data(), data.size());
}
std::array<uint8_t, 32> sha256(const std::string& data) {
    return sha256(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}
std::array<uint8_t, 32> sha256d(const uint8_t* data, size_t n) {
    auto h = sha256(data, n);
    return sha256(h.data(), h.size());
}
std::array<uint8_t, 32> sha256d(const std::vector<uint8_t>& data) {
    return sha256d(data.data(), data.size());
}

std::string to_hex(const uint8_t* p, size_t n) {
    static const char* h = "0123456789abcdef";
    std::string o;
    o.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        o[2 * i] = h[p[i] >> 4];
        o[2 * i + 1] = h[p[i] & 0xf];
    }
    return o;
}
std::string to_hex(const std::array<uint8_t, 32>& a) { return to_hex(a.data(), a.size()); }

std::optional<std::vector<uint8_t>> from_hex(const std::string& s) {
    if (s.size() % 2) return std::nullopt;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<uint8_t> o(s.size() / 2);
    for (size_t i = 0; i < o.size(); ++i) {
        int hi = nibble(s[2 * i]);
        int lo = nibble(s[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        o[i] = uint8_t((hi << 4) | lo);
    }
    return o;
}
