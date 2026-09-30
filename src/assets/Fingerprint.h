#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace game {
std::string fingerprint(const std::filesystem::path& source, std::string_view settings);
}
