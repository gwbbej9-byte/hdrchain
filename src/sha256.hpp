#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t n);
std::array<uint8_t, 32> sha256(const std::vector<uint8_t>& data);
std::array<uint8_t, 32> sha256(const std::string& data);
std::array<uint8_t, 32> sha256d(const uint8_t* data, size_t n);
std::array<uint8_t, 32> sha256d(const std::vector<uint8_t>& data);

std::string to_hex(const uint8_t* p, size_t n);
std::string to_hex(const std::array<uint8_t, 32>& a);
std::optional<std::vector<uint8_t>> from_hex(const std::string& s);
