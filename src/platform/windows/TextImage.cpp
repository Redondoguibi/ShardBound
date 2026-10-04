#include "platform/TextImage.h"
#include <windows.h>
#include <algorithm>
#include <stdexcept>
namespace game {
namespace {
std::wstring utf8(const std::string& text) {
    if(text.empty()) return {};
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(!count) throw std::runtime_error("Invalid UTF-8 UI text");
    std::wstring result(static_cast<size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),count); return result;
}
}
std::vector<std::uint8_t> gamePanelImage(const std::string& text,bool map,const std::vector<UiMarker>& markers,bool expanded) {
    const auto wideText=utf8(text);
    std::vector<std::wstring> labels; for(const auto& marker:markers) labels.push_back(utf8(marker.label));
    const int width=600,height=(map || expanded) ? 440 : 156;
    HDC dc=CreateCompatibleDC(nullptr); if(!dc) throw std::runtime_error("Cannot create UI DC");
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr; auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap) { DeleteDC(dc); throw std::runtime_error("Cannot create UI bitmap"); }
    auto old=SelectObject(dc,bitmap); RECT area{0,0,width,height}; auto brush=CreateSolidBrush(RGB(14,26,32)); FillRect(dc,&area,brush); DeleteObject(brush);
    auto font=CreateFontW(19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto oldFont=SelectObject(dc,font); SetTextColor(dc,RGB(221,237,229)); SetBkMode(dc,TRANSPARENT);
    RECT textArea{14,8,590,expanded && !map ? 432 : 150}; DrawTextW(dc,wideText.c_str(),static_cast<int>(wideText.size()),&textArea,DT_LEFT|DT_NOPREFIX);
    if(map) {
        RECT ground{28,168,572,422}; brush=CreateSolidBrush(RGB(35,64,47)); FillRect(dc,&ground,brush); DeleteObject(brush);
        float minX=0,maxX=0,minZ=0,maxZ=0;
        if(!markers.empty()) minX=maxX=markers.front().x,minZ=maxZ=markers.front().z;
        for(const auto& marker:markers) { minX=std::min(minX,marker.x); maxX=std::max(maxX,marker.x); minZ=std::min(minZ,marker.z); maxZ=std::max(maxZ,marker.z); }
        const float scale=std::min(480.0f/std::max(8.0f,maxX-minX),210.0f/std::max(8.0f,maxZ-minZ));
        const float centerX=(minX+maxX)*.5f,centerZ=(minZ+maxZ)*.5f;
        std::vector<RECT> placedLabels;
        for(size_t i=0;i<markers.size();++i) {
            const auto& marker=markers[i];
            const int x=std::clamp(int(300+(marker.x-centerX)*scale),32,562),y=std::clamp(int(295-(marker.z-centerZ)*scale),174,408);
            RECT dot{x-4,y-4,x+4,y+4}; brush=CreateSolidBrush(marker.active ? RGB(97,228,199) : RGB(122,130,125)); FillRect(dc,&dot,brush); DeleteObject(brush);
            SIZE extent{}; GetTextExtentPoint32W(dc,labels[i].c_str(),static_cast<int>(labels[i].size()),&extent);
            const int labelWidth=std::min(260,int(extent.cx)+4);
            const int left=std::clamp(x+7,32,572-labelWidth);
            for(int attempt=0;attempt<15;++attempt) {
                const int offset=attempt==0 ? 0 : ((attempt+1)/2)*22*(attempt%2 ? 1 : -1);
                const int top=std::clamp(y-10+offset,170,400);
                RECT label{left,top,left+labelWidth,top+22}; bool overlaps=false;
                for(const auto& placed:placedLabels) { RECT intersection{}; if(IntersectRect(&intersection,&label,&placed)) { overlaps=true; break; } }
                if(!overlaps) {
                    placedLabels.push_back(label);
                    DrawTextW(dc,labels[i].c_str(),static_cast<int>(labels[i].size()),&label,DT_LEFT|DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS);
                    break;
                }
            }
        }
    }
    GdiFlush(); const auto* bytes=static_cast<const std::uint8_t*>(pixels); std::vector<std::uint8_t> result(size_t(width)*height*4);
    for(size_t i=0;i<result.size();i+=4) { result[i]=bytes[i+2]; result[i+1]=bytes[i+1]; result[i+2]=bytes[i]; result[i+3]=255; }
    SelectObject(dc,oldFont); DeleteObject(font); SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); return result;
}
std::vector<std::uint8_t> preparationImage(const std::string& message,size_t done,size_t total) {
    constexpr int width=640,height=240;
    HDC dc=CreateCompatibleDC(nullptr);
    if (!dc) throw std::runtime_error("Cannot create preparation text DC");
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr; HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if (!bitmap) { DeleteDC(dc); throw std::runtime_error("Cannot create preparation text bitmap"); }
    const auto old=SelectObject(dc,bitmap);
    RECT area{0,0,width,height}; const auto background=CreateSolidBrush(RGB(12,18,30)); FillRect(dc,&area,background); DeleteObject(background);
    auto font=CreateFontW(30,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    const auto oldFont=SelectObject(dc,font); SetTextColor(dc,RGB(235,242,250)); SetBkMode(dc,TRANSPARENT);
    const std::string text="Preparing Mods\n"+message+"\n"+std::to_string(done)+" / "+std::to_string(total);
    RECT textArea{24,20,616,180}; DrawTextA(dc,text.c_str(),static_cast<int>(text.size()),&textArea,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);
    RECT bar{24,200,24+int(592.0*(total ? std::min(1.0,double(done)/double(total)) : 0)),215};
    auto fill=CreateSolidBrush(RGB(70,195,165)); FillRect(dc,&bar,fill); DeleteObject(fill); GdiFlush();
    const auto* bytes=static_cast<const std::uint8_t*>(pixels);
    std::vector<std::uint8_t> output(width*height*4);
    for(size_t i=0;i<output.size();i+=4) { output[i]=bytes[i+2]; output[i+1]=bytes[i+1]; output[i+2]=bytes[i]; output[i+3]=255; }
    SelectObject(dc,oldFont); DeleteObject(font); SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc);
    return output;
}
}
