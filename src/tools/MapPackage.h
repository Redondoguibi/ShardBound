#pragma once
#include <filesystem>

namespace game {
class FiniteMap;
void exportMapPackage(const FiniteMap& map,const std::filesystem::path& content,
    const std::filesystem::path& destination,const std::filesystem::path& modcheck);
}
