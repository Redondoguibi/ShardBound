#pragma once
#include "world/FiniteMap.h"

namespace game {
inline constexpr size_t editorCellEditLimit=FiniteMap::maxSide*FiniteMap::maxSide;
struct EditorCellEdit { int x,y,z;std::string before,after; };
struct EditorEdit {
    std::vector<EditorCellEdit> cells;
    std::optional<std::pair<FiniteMap::Spawn,FiniteMap::Spawn>> spawn;
    std::wstring label;
    std::optional<std::pair<std::optional<FiniteMap::Spawn>,std::optional<FiniteMap::Spawn>>> exit;
    std::optional<FiniteMap::Interior> interior;
    std::optional<FiniteMap::Prop> prop;
    std::optional<std::pair<std::shared_ptr<const FiniteMap>,std::shared_ptr<const FiniteMap>>> revision;
    std::optional<std::pair<std::filesystem::path,std::filesystem::path>> content;
};
struct EditorTimeline {
    std::vector<EditorEdit> edits;
    size_t cursor=0;
};

void applyEditorMapEdit(FiniteMap& map,const WorldDefinition& catalog,const EditorEdit& edit,bool forward);
std::string writeEditorTimeline(const std::filesystem::path& project,const std::vector<EditorEdit>& edits,size_t cursor);
EditorTimeline readEditorTimeline(const std::filesystem::path& project,const std::string& file,
    const FiniteMap& map,const std::filesystem::path& content,const WorldDefinition& catalog);
}
