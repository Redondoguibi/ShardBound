#include "platform/TextImage.h"
#include <windows.h>
#include <algorithm>
#include <stdexcept>
namespace game {
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
