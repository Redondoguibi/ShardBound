#pragma once
#include <filesystem>
#include <string>

namespace game {
inline std::string pathText(const std::filesystem::path& path) {
    const auto utf8=path.u8string();
    return {utf8.begin(),utf8.end()};
}
}
