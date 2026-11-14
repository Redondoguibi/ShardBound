#pragma once
#include "tools/EditorAi.h"
#include "world/FiniteMap.h"
#include <windows.h>
#include <optional>

namespace game {
struct AiMapRevision { FiniteMap map; std::string label;std::filesystem::path content; };
struct AiDraft { std::vector<AiMapRevision> revisions; };
AiDraft authorMapWithAi(HWND parent,const FiniteMap& map,const WorldDefinition& catalog,
    const std::filesystem::path& mods,const std::filesystem::path& programDirectory,const std::filesystem::path& bridge={});
}
