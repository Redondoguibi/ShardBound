#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace game {
std::vector<std::uint8_t> preparationImage(const std::string& message,size_t done,size_t total);
}
