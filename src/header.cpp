// language: C++, file: header.cpp, target: Windows/Linux, C++20
// *80-byte header like Bitcoin. PoW = sha256d(header) as LE uint256 < compact target.*
#include "header.hpp"

#include <cstring>
#include <stdexcept>

namespace {

void wr_le32(uint8_t* p, uint32_t x) {
    p[0] = uint8_t(x); p[1] = uint8_t(x >> 8); p[2] = uint8_t(x >> 16); p[3] = uint8_t(x >> 24);
}
uint32_t rd_le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

}  // namespace

std::array<uint8_t, kHeaderSize> BlockHeader::serialize() const {
    std::array<uint8_t, kHeaderSize> o{};
    wr_le32(o.data(), version);
    std::memcpy(o.data() + 4, prev.data(), 32);
    std::memcpy(o.data() + 36, merkle.data(), 32);
    wr_le32(o.data() + 68, time);
    wr_le32(o.data() + 72, bits);
    wr_le32(o.data() + 76, nonce);
    return o;
}

BlockHeader BlockHeader::deserialize(const std::array<uint8_t, kHeaderSize>& p) {
    BlockHeader h;
    h.version = rd_le32(p.data());
    std::memcpy(h.prev.data(), p.data() + 4, 32);
    std::memcpy(h.merkle.data(), p.data() + 36, 32);
    h.time = rd_le32(p.data() + 68);
    h.bits = rd_le32(p.data() + 72);
    h.nonce = rd_le32(p.data() + 76);
    return h;
}

std::array<uint8_t, 32> BlockHeader::pow_hash() const {
    auto b = serialize();
    return sha256d(b.data(), b.size());
}

std::array<uint8_t, 32> bits_to_target(uint32_t bits) {
    std::array<uint8_t, 32> t{};
    int exp = int(bits >> 24);
    uint32_t man = bits & 0x007fffff;
    if (bits & 0x00800000) man = 0;  // negative compact → zero target, never met
    if (exp <= 0) return t;
    // mantissa is 3 bytes; place at BE offset 32 - exp
    uint8_t m[3] = {uint8_t(man >> 16), uint8_t(man >> 8), uint8_t(man)};
    int pos = 32 - exp;
    for (int i = 0; i < 3; ++i) {
        int idx = pos + i;
        if (idx >= 0 && idx < 32) t[size_t(idx)] = m[i];
    }
    return t;
}

bool hash_meets_target(const std::array<uint8_t, 32>& hash, uint32_t bits) {
    // Bitcoin: hash bytes are little-endian uint256. Compare as big-endian magnitude.
    auto target = bits_to_target(bits);
    uint8_t be[32];
    for (int i = 0; i < 32; ++i) be[i] = hash[size_t(31 - i)];
    return std::memcmp(be, target.data(), 32) <= 0;
}

bool mine(BlockHeader& hdr, uint32_t max_tries) {
    for (uint32_t i = 0; i < max_tries; ++i) {
        hdr.nonce = i;
        if (hash_meets_target(hdr.pow_hash(), hdr.bits)) return true;
    }
    return false;
}

std::array<uint8_t, 32> merkle_root(const std::vector<std::vector<uint8_t>>& txs) {
    if (txs.empty()) throw std::runtime_error("merkle: empty tx list");
    std::vector<std::array<uint8_t, 32>> lvl;
    lvl.reserve(txs.size());
    for (const auto& tx : txs) lvl.push_back(sha256d(tx));
    while (lvl.size() > 1) {
        if (lvl.size() % 2) lvl.push_back(lvl.back());
        std::vector<std::array<uint8_t, 32>> next;
        next.reserve(lvl.size() / 2);
        for (size_t i = 0; i < lvl.size(); i += 2) {
            uint8_t cat[64];
            std::memcpy(cat, lvl[i].data(), 32);
            std::memcpy(cat + 32, lvl[i + 1].data(), 32);
            next.push_back(sha256d(cat, 64));
        }
        lvl.swap(next);
    }
    return lvl[0];
}

std::array<uint8_t, 32> merkle_root_strings(const std::vector<std::string>& txs) {
    std::vector<std::vector<uint8_t>> raw;
    raw.reserve(txs.size());
    for (const auto& t : txs)
        raw.emplace_back(t.begin(), t.end());
    return merkle_root(raw);
}
