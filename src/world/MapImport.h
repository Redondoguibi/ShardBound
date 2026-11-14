#pragma once
#include "world/FiniteMap.h"

namespace game {
FiniteMap importMapFromSave(const std::filesystem::path& checkpoint, FiniteMap::Bounds selection);
}
