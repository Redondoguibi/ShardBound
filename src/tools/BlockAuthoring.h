#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace game {
struct AuthoredBlock {
    std::string mod;
    std::filesystem::path model;
    bool solid=true;
    std::array<std::uint32_t,256> pixels{}; // RGB, 16 by 16, top row first.
    std::string name;
    std::string project;
};
void publishBlock(const std::filesystem::path& mods,const AuthoredBlock& block);
}
