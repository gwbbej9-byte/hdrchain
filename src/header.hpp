#pragma once
#include "sha256.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr size_t kHeaderSize = 80;
inline constexpr uint32_t kEasyBits = 0x1f0fffff;  // laptop mine in milliseconds

struct BlockHeader {
    uint32_t version = 1;
    std::array<uint8_t, 32> prev{};
    std::array<uint8_t, 32> merkle{};
    uint32_t time = 0;
    uint32_t bits = 0;
    uint32_t nonce = 0;

    std::array<uint8_t, kHeaderSize> serialize() const;
    static BlockHeader deserialize(const std::array<uint8_t, kHeaderSize>& p);
    std::array<uint8_t, 32> pow_hash() const;  // sha256d of 80-byte header
};

std::array<uint8_t, 32> bits_to_target(uint32_t bits);
bool hash_meets_target(const std::array<uint8_t, 32>& hash, uint32_t bits);
bool mine(BlockHeader& hdr, uint32_t max_tries = 4000000);

std::array<uint8_t, 32> merkle_root(const std::vector<std::vector<uint8_t>>& txs);
std::array<uint8_t, 32> merkle_root_strings(const std::vector<std::string>& txs);
