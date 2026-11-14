#pragma once
#include "tools/BlockAuthoring.h"
#include <optional>
#include <windows.h>

namespace game {
std::optional<AuthoredBlock> editNewBlock(HWND parent);
}
