#pragma once
#include "header.hpp"

#include <optional>
#include <string>
#include <vector>

struct Block {
    BlockHeader header;
    std::vector<std::string> txs;
};

struct Chain {
    std::vector<Block> blocks;
};

enum class Check { Ok, Empty, BadLink, BadMerkle, BadPow, BadFormat, IoError };

Check verify(const Chain& c);
std::string check_str(Check c);

struct LoadResult {
    Check status = Check::BadFormat;
    Chain chain;
};

LoadResult load_file(const std::string& path);
Check save_file(const std::string& path, const Chain& c);

std::optional<Block> make_block(const std::array<uint8_t, 32>& prev, const std::vector<std::string>& txs,
                                uint32_t bits, uint32_t time);
