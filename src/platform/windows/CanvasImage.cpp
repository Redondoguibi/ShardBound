#include "platform/TextImage.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
namespace game {
namespace {
constexpr int width=1280,height=720;
struct Surface {
    HDC dc=CreateCompatibleDC(nullptr); HBITMAP bitmap=nullptr; HGDIOBJ old=nullptr; void* bytes=nullptr;
    std::map<int,HFONT> fonts;
    Surface() {
        if(!dc) throw std::runtime_error("Cannot create canvas DC");
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bytes,nullptr,0);
        if(!bitmap) { DeleteDC(dc); dc=nullptr; throw std::runtime_error("Cannot create canvas bitmap"); }
        old=SelectObject(dc,bitmap); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    }
    ~Surface() { for(auto [size,font]:fonts) { (void)size; DeleteObject(font); } if(dc) { SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); } }
    HFONT font(int size) {
        if(auto it=fonts.find(size);it!=fonts.end()) return it->second;
        auto value=CreateFontW(-size,0,0,0,size>=24 ? FW_SEMIBOLD : FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");
        if(!value) throw std::runtime_error("Cannot create canvas font"); fonts[size]=value; return value;
    }
};
}
std::vector<std::uint8_t> canvasImage(const std::vector<CanvasCommand>& commands) {
    std::vector<std::uint8_t> result(width*height*4,0); Surface surface;
    auto over=[&](int x,int y,const std::array<unsigned char,4>& color,float coverage) {
        const size_t i=(size_t(y)*width+x)*4; const float alpha=color[3]/255.0f*coverage,previous=result[i+3]/255.0f,total=alpha+previous*(1-alpha);
        if(total<=0) return;
        for(size_t c=0;c<3;++c) result[i+c]=static_cast<std::uint8_t>(std::clamp((color[c]*alpha+result[i+c]*previous*(1-alpha))/total,0.0f,255.0f));
        result[i+3]=static_cast<std::uint8_t>(total*255+.5f);
    };
    for(const auto& command:commands) {
        const int left=std::clamp(command.x,0,width),top=std::clamp(command.y,0,height);
        const int right=std::clamp(command.x+command.w,0,width),bottom=std::clamp(command.y+command.h,0,height);
        if(left>=right || top>=bottom) continue;
        if(command.kind=="text") {
            const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,command.text.data(),int(command.text.size()),nullptr,0);
            if(!count && !command.text.empty()) throw std::runtime_error("Invalid UTF-8 canvas text");
            std::wstring text(size_t(count),L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,command.text.data(),int(command.text.size()),text.data(),count);
            RECT area{left,top,right,bottom}; FillRect(surface.dc,&area,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            auto oldFont=SelectObject(surface.dc,surface.font(command.size));
            UINT flags=DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER;
            flags|=command.align=="center" ? DT_CENTER : command.align=="right" ? DT_RIGHT : DT_LEFT;
            DrawTextW(surface.dc,text.c_str(),count,&area,flags); GdiFlush();
            const auto* pixels=static_cast<const std::uint8_t*>(surface.bytes);
            for(int y=top;y<bottom;++y) for(int x=left;x<right;++x) { const auto i=(size_t(y)*width+x)*4; if(pixels[i]) over(x,y,command.color,pixels[i]/255.0f); }
            SelectObject(surface.dc,oldFont);
        } else {
            for(int y=top;y<bottom;++y) for(int x=left;x<right;++x) {
                if(command.kind=="diamond" && std::abs((x+.5f-command.x)/command.w-.5f)+std::abs((y+.5f-command.y)/command.h-.5f)>.5f) continue;
                over(x,y,command.color,1);
            }
        }
    }
    return result;
}
}
