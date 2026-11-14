#pragma once
#include <filesystem>
#include <string_view>

namespace game {
std::filesystem::path createEditorRevision(std::string_view label);
}
