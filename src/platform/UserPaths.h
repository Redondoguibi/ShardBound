#pragma once
#include <filesystem>
namespace game {
std::filesystem::path defaultUserDirectory(const std::filesystem::path& installation,bool portable=false);
}
