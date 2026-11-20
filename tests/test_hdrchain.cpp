// language: C++, file: test_hdrchain.cpp, target: Windows/Linux, C++20
#include "chain.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

static int g_fail = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);                                        \
            g_fail++;                                                                                                  \
        }                                                                                                              \
    } while (0)

static void test_sha256_nist() {
    CHECK(to_hex(sha256(std::string())) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(to_hex(sha256(std::string("abc"))) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(to_hex(sha256(std::string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    std::string a(1000000, 'a');
    CHECK(to_hex(sha256(a)) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

static void test_sha256d_known() {
    auto d = sha256d(reinterpret_cast<const uint8_t*>(""), 0);
    CHECK(to_hex(d) == "5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456");
}

static void test_merkle() {
    auto one = merkle_root_strings({"only"});
    auto leaf = sha256d(std::vector<uint8_t>({'o', 'n', 'l', 'y'}));
    CHECK(one == leaf);

    auto two = merkle_root_strings({"a", "b"});
    uint8_t cat[64];
    auto ha = sha256d(std::vector<uint8_t>({'a'}));
    auto hb = sha256d(std::vector<uint8_t>({'b'}));
    std::memcpy(cat, ha.data(), 32);
    std::memcpy(cat + 32, hb.data(), 32);
    CHECK(two == sha256d(cat, 64));

    auto three = merkle_root_strings({"a", "b", "c"});
    auto hc = sha256d(std::vector<uint8_t>({'c'}));
    uint8_t cat2[64];
    std::memcpy(cat2, hc.data(), 32);
    std::memcpy(cat2 + 32, hc.data(), 32);
    auto mid = sha256d(cat2, 64);
    uint8_t cat3[64];
    std::memcpy(cat3, two.data(), 32);
    std::memcpy(cat3 + 32, mid.data(), 32);
    CHECK(three == sha256d(cat3, 64));

    auto tampered = merkle_root_strings({"a", "X"});
    CHECK(tampered != two);
}

static void test_empty_chain() {
    Chain empty;
    CHECK(verify(empty) == Check::Empty);
}

static void test_genesis_prev_zeros() {
    std::array<uint8_t, 32> z{};
    auto b0 = make_block(z, {"genesis"}, kEasyBits, 1700000000);
    CHECK(b0.has_value());
    CHECK(b0->header.prev == z);
    CHECK(hash_meets_target(b0->header.pow_hash(), kEasyBits));
    CHECK(b0->header.merkle == merkle_root_strings({"genesis"}));

    Chain ok{{*b0}};
    CHECK(verify(ok) == Check::Ok);

    Chain bad_prev = ok;
    bad_prev.blocks[0].header.prev[0] = 1;
    CHECK(verify(bad_prev) == Check::BadLink);
}

static void test_append_link() {
    std::array<uint8_t, 32> z{};
    auto b0 = make_block(z, {"genesis"}, kEasyBits, 1700000000);
    CHECK(b0.has_value());
    auto h0 = b0->header.pow_hash();
    auto b1 = make_block(h0, {"pay-alice", "pay-bob"}, kEasyBits, 1700000060);
    CHECK(b1.has_value());
    CHECK(b1->header.prev == h0);

    Chain ok{{*b0, *b1}};
    CHECK(verify(ok) == Check::Ok);

    Chain broken_link = ok;
    broken_link.blocks[1].header.prev[0] ^= 0xff;
    CHECK(verify(broken_link) == Check::BadLink);

    Chain broken_merkle = ok;
    broken_merkle.blocks[0].txs[0] = "not-genesis";
    CHECK(verify(broken_merkle) == Check::BadMerkle);

    Chain broken_pow = ok;
    broken_pow.blocks[1].header.nonce += 1;
    CHECK(verify(broken_pow) == Check::BadPow);

    BlockHeader nonce_flip = b0->header;
    nonce_flip.nonce ^= 1u;
    CHECK(!hash_meets_target(nonce_flip.pow_hash(), kEasyBits));
}

static void write_bytes(const std::filesystem::path& p, std::string_view s) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

static void test_file_roundtrip() {
    namespace fs = std::filesystem;
    auto p = fs::temp_directory_path() / "hdrchain-test.hdrc";
    std::array<uint8_t, 32> z{};
    Chain c;
    auto b0 = make_block(z, {"alpha"}, kEasyBits, 1);
    CHECK(b0.has_value());
    c.blocks.push_back(std::move(*b0));
    auto b1 = make_block(c.blocks[0].header.pow_hash(), {"beta"}, kEasyBits, 2);
    CHECK(b1.has_value());
    c.blocks.push_back(std::move(*b1));
    CHECK(save_file(p.string(), c) == Check::Ok);
    auto d = load_file(p.string());
    CHECK(d.status == Check::Ok);
    CHECK(verify(d.chain) == Check::Ok);
    CHECK(d.chain.blocks.size() == 2);
    CHECK(d.chain.blocks[0].txs[0] == "alpha");
    CHECK(d.chain.blocks[1].txs[0] == "beta");
    CHECK(d.chain.blocks[0].header.pow_hash() == c.blocks[0].header.pow_hash());
    CHECK(d.chain.blocks[1].header.prev == d.chain.blocks[0].header.pow_hash());
    fs::remove(p);
}

static void test_truncated_file() {
    namespace fs = std::filesystem;
    auto p = fs::temp_directory_path() / "hdrchain-trunc.hdrc";
    write_bytes(p, "HDRC1\n00ffaabb");  // short header hex
    auto r = load_file(p.string());
    CHECK(r.status == Check::BadFormat);
    fs::remove(p);

    write_bytes(p, "HDRC1\n");
    auto empty_ok = load_file(p.string());
    CHECK(empty_ok.status == Check::Ok);
    CHECK(verify(empty_ok.chain) == Check::Empty);
    fs::remove(p);

    write_bytes(p, "");
    auto empty_file = load_file(p.string());
    CHECK(empty_file.status == Check::BadFormat);
    fs::remove(p);
}

static void test_garbage_file() {
    namespace fs = std::filesystem;
    auto p = fs::temp_directory_path() / "hdrchain-garbage.hdrc";
    write_bytes(p, "\xff\xfe\x00 not a chain");
    auto r = load_file(p.string());
    CHECK(r.status == Check::BadFormat);
    fs::remove(p);

    write_bytes(p, "HDRC2\n");
    auto mag = load_file(p.string());
    CHECK(mag.status == Check::BadFormat);
    fs::remove(p);

    write_bytes(p, "HDRC1\nzzzz\n");
    auto hex = load_file(p.string());
    CHECK(hex.status == Check::BadFormat);
    fs::remove(p);

    auto missing = load_file((fs::temp_directory_path() / "hdrchain-no-such.hdrc").string());
    CHECK(missing.status == Check::IoError);
}

int main() {
    test_sha256_nist();
    test_sha256d_known();
    test_merkle();
    test_empty_chain();
    test_genesis_prev_zeros();
    test_append_link();
    test_file_roundtrip();
    test_truncated_file();
    test_garbage_file();
    if (g_fail) {
        std::fprintf(stderr, "%d checks failed\n", g_fail);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
