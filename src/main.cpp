// language: C++, file: main.cpp, target: Windows/Linux, C++20
// *CLI: genesis | append | verify | show. Easy default bits so mine finishes in ms.*
#include "chain.hpp"

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static uint32_t now_unix() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return uint32_t(t / 10000000ull - 11644473600ull);
}
#else
#include <ctime>
static uint32_t now_unix() { return uint32_t(std::time(nullptr)); }
#endif

static void usage() {
    std::fprintf(stderr,
                 "hdrchain — SHA-256d header chain\n"
                 "  hdrchain genesis <out.hdrc> [tx...]\n"
                 "  hdrchain append <chain.hdrc> <tx...>\n"
                 "  hdrchain verify <chain.hdrc>\n"
                 "  hdrchain show   <chain.hdrc>\n");
}

static std::vector<std::string> txs_from(int argc, char** argv, int i) {
    std::vector<std::string> txs;
    for (; i < argc; ++i) txs.emplace_back(argv[i]);
    if (txs.empty()) txs.emplace_back("coinbase");
    return txs;
}

static int fail_check(Check c) {
    std::fprintf(stderr, "error: %s\n", check_str(c).c_str());
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    std::string cmd = argv[1];
    if (cmd == "genesis") {
        if (argc < 3) {
            usage();
            return 2;
        }
        auto txs = txs_from(argc, argv, 3);
        Chain c;
        std::array<uint8_t, 32> z{};
        auto b = make_block(z, txs, kEasyBits, now_unix());
        if (!b) return fail_check(Check::BadPow);
        c.blocks.push_back(std::move(*b));
        auto st = save_file(argv[2], c);
        if (st != Check::Ok) return fail_check(st);
        auto h = c.blocks[0].header.pow_hash();
        std::printf("genesis %s nonce=%u\n", to_hex(h).c_str(), c.blocks[0].header.nonce);
        return 0;
    }
    if (cmd == "append") {
        if (argc < 3) {
            usage();
            return 2;
        }
        auto loaded = load_file(argv[2]);
        if (loaded.status != Check::Ok) return fail_check(loaded.status);
        if (loaded.chain.blocks.empty()) return fail_check(Check::Empty);
        auto prev = loaded.chain.blocks.back().header.pow_hash();
        auto txs = txs_from(argc, argv, 3);
        auto b = make_block(prev, txs, kEasyBits, now_unix());
        if (!b) return fail_check(Check::BadPow);
        loaded.chain.blocks.push_back(std::move(*b));
        auto st = save_file(argv[2], loaded.chain);
        if (st != Check::Ok) return fail_check(st);
        auto h = loaded.chain.blocks.back().header.pow_hash();
        std::printf("block %zu %s nonce=%u\n", loaded.chain.blocks.size() - 1, to_hex(h).c_str(),
                    loaded.chain.blocks.back().header.nonce);
        return 0;
    }
    if (cmd == "verify") {
        if (argc < 3) {
            usage();
            return 2;
        }
        auto loaded = load_file(argv[2]);
        if (loaded.status != Check::Ok) {
            std::printf("%s (%zu blocks)\n", check_str(loaded.status).c_str(), loaded.chain.blocks.size());
            return 1;
        }
        auto r = verify(loaded.chain);
        std::printf("%s (%zu blocks)\n", check_str(r).c_str(), loaded.chain.blocks.size());
        return r == Check::Ok ? 0 : 1;
    }
    if (cmd == "show") {
        if (argc < 3) {
            usage();
            return 2;
        }
        auto loaded = load_file(argv[2]);
        if (loaded.status != Check::Ok) return fail_check(loaded.status);
        const auto& c = loaded.chain;
        for (size_t i = 0; i < c.blocks.size(); ++i) {
            const auto& b = c.blocks[i];
            auto h = b.header.pow_hash();
            std::printf("#%zu hash=%s nonce=%u bits=%08x txs=%zu merkle=%s\n", i, to_hex(h).c_str(),
                        b.header.nonce, b.header.bits, b.txs.size(), to_hex(b.header.merkle).c_str());
            for (const auto& t : b.txs) std::printf("    tx %s\n", t.c_str());
        }
        return 0;
    }
    usage();
    return 2;
}
