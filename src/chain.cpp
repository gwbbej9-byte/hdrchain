// language: C++, file: chain.cpp, target: Windows/Linux, C++20
#include "chain.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr std::string_view kMagic = "HDRC1";
constexpr size_t kHeaderHexLen = kHeaderSize * 2;
constexpr size_t kMaxFileBytes = 8 * 1024 * 1024;

std::string_view strip_cr(std::string_view s) {
    if (!s.empty() && s.back() == '\r') s.remove_suffix(1);
    return s;
}

bool atomic_replace(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    return !ec;
#endif
}

LoadResult parse_hdrc1(std::string_view body) {
    LoadResult out;
    size_t i = 0;
    auto next_line = [&]() -> std::optional<std::string_view> {
        if (i > body.size()) return std::nullopt;
        if (i == body.size()) {
            i = body.size() + 1;
            return std::string_view{};
        }
        size_t nl = body.find('\n', i);
        std::string_view line;
        if (nl == std::string_view::npos) {
            line = body.substr(i);
            i = body.size() + 1;
        } else {
            line = body.substr(i, nl - i);
            i = nl + 1;
        }
        return strip_cr(line);
    };

    auto magic = next_line();
    if (!magic || *magic != kMagic) {
        out.status = Check::BadFormat;
        return out;
    }

    Chain c;
    for (;;) {
        auto maybe = next_line();
        if (!maybe) break;
        auto line = *maybe;
        if (line.empty()) continue;
        std::istringstream in{std::string(line)};
        std::string hexh;
        if (!(in >> hexh) || hexh.size() != kHeaderHexLen) {
            out.status = Check::BadFormat;
            return out;
        }
        auto hb = from_hex(hexh);
        if (!hb || hb->size() != kHeaderSize) {
            out.status = Check::BadFormat;
            return out;
        }
        std::array<uint8_t, kHeaderSize> raw{};
        std::memcpy(raw.data(), hb->data(), kHeaderSize);
        Block b;
        b.header = BlockHeader::deserialize(raw);
        std::string txh;
        while (in >> txh) {
            auto t = from_hex(txh);
            if (!t) {
                out.status = Check::BadFormat;
                return out;
            }
            b.txs.emplace_back(t->begin(), t->end());
        }
        c.blocks.push_back(std::move(b));
    }
    out.status = Check::Ok;
    out.chain = std::move(c);
    return out;
}

}  // namespace

Check verify(const Chain& c) {
    if (c.blocks.empty()) return Check::Empty;
    for (size_t i = 0; i < c.blocks.size(); ++i) {
        const auto& b = c.blocks[i];
        if (b.txs.empty()) return Check::BadMerkle;
        auto mr = merkle_root_strings(b.txs);
        if (mr != b.header.merkle) return Check::BadMerkle;
        if (i == 0) {
            std::array<uint8_t, 32> z{};
            if (b.header.prev != z) return Check::BadLink;
        } else {
            auto want = c.blocks[i - 1].header.pow_hash();
            if (b.header.prev != want) return Check::BadLink;
        }
        if (!hash_meets_target(b.header.pow_hash(), b.header.bits)) return Check::BadPow;
    }
    return Check::Ok;
}

std::string check_str(Check c) {
    switch (c) {
    case Check::Ok: return "ok";
    case Check::Empty: return "empty chain";
    case Check::BadLink: return "prev_hash mismatch";
    case Check::BadMerkle: return "merkle root mismatch";
    case Check::BadPow: return "proof-of-work failed";
    case Check::BadFormat: return "bad file format";
    case Check::IoError: return "io error";
    }
    return "unknown";
}

std::optional<Block> make_block(const std::array<uint8_t, 32>& prev, const std::vector<std::string>& txs,
                                uint32_t bits, uint32_t time) {
    if (txs.empty()) return std::nullopt;
    Block b;
    b.txs = txs;
    b.header.prev = prev;
    b.header.merkle = merkle_root_strings(txs);
    b.header.bits = bits;
    b.header.time = time;
    b.header.version = 1;
    if (!mine(b.header)) return std::nullopt;
    return b;
}

Check save_file(const std::string& path, const Chain& c) {
    namespace fs = std::filesystem;
    fs::path dest(path);
    fs::path tmp = dest;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return Check::IoError;
        f << kMagic << '\n';
        for (const auto& b : c.blocks) {
            auto raw = b.header.serialize();
            f << to_hex(raw.data(), raw.size());
            for (const auto& tx : b.txs) {
                f << ' ' << to_hex(reinterpret_cast<const uint8_t*>(tx.data()), tx.size());
            }
            f << '\n';
        }
        f.flush();
        if (!f) {
            f.close();
            fs::remove(tmp);
            return Check::IoError;
        }
    }
    if (!atomic_replace(tmp, dest)) {
        fs::remove(tmp);
        return Check::IoError;
    }
    return Check::Ok;
}

LoadResult load_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        LoadResult r;
        r.status = Check::IoError;
        return r;
    }
    auto end = f.tellg();
    if (end < 0) {
        LoadResult r;
        r.status = Check::IoError;
        return r;
    }
    const auto sz = static_cast<std::uintmax_t>(end);
    if (sz > kMaxFileBytes) {
        LoadResult r;
        r.status = Check::BadFormat;
        return r;
    }
    f.seekg(0, std::ios::beg);
    std::string buf;
    buf.resize(static_cast<size_t>(sz));
    if (sz > 0 && !f.read(buf.data(), static_cast<std::streamsize>(sz))) {
        LoadResult r;
        r.status = Check::IoError;
        return r;
    }
    if (static_cast<std::uintmax_t>(f.gcount()) != sz && sz > 0) {
        LoadResult r;
        r.status = Check::BadFormat;
        return r;
    }
    return parse_hdrc1(buf);
}
