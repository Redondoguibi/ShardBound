#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace game {
struct UiMarker { float x,z; std::string label; bool active; bool operator==(const UiMarker&) const = default; };
std::vector<std::uint8_t> gamePanelImage(const std::string& text,bool map,const std::vector<UiMarker>& markers,bool expanded=false);
std::vector<std::uint8_t> preparationImage(const std::string& message,size_t done,size_t total);
}
