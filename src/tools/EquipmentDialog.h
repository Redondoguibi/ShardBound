#pragma once
#include "tools/EquipmentAuthoring.h"
#include <optional>
#include <windows.h>

namespace game {
std::optional<AuthoredEquipment> editEquipment(HWND parent,const std::filesystem::path& programDirectory,const std::filesystem::path& bridge={});
}
