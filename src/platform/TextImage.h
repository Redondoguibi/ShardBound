#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <vector>
namespace game {
struct CanvasCommand {
    std::string kind,text,align;
    int x=0,y=0,w=0,h=0,size=18;
    std::array<unsigned char,4> color{255,255,255,255};
    int clipX=0,clipY=0,clipW=1280,clipH=720;
    bool operator==(const CanvasCommand&) const = default;
};
std::vector<std::uint8_t> canvasImage(const std::vector<CanvasCommand>& commands);
std::vector<std::pair<size_t,int>> textAdvances(const std::string& text,int size=20);
struct UiMarker { float x,z; std::string label; bool active; bool operator==(const UiMarker&) const = default; };
std::vector<std::uint8_t> gamePanelImage(const std::string& text,bool map,const std::vector<UiMarker>& markers,bool expanded=false);
std::vector<std::uint8_t> preparationImage(const std::string& message,size_t done,size_t total);
}
