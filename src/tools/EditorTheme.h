#pragma once
#include <windows.h>
#include <string_view>

namespace game::editor {
inline constexpr auto background=RGB(20,22,27);
inline constexpr auto panel=RGB(27,30,36);
inline constexpr auto field=RGB(35,39,47);
inline constexpr auto line=RGB(49,55,65);
inline constexpr auto canvas=RGB(16,19,24);
inline constexpr auto ink=RGB(234,238,244);
inline constexpr auto muted=RGB(155,168,185);
inline constexpr auto accent=RGB(127,190,174);
inline constexpr auto selected=RGB(35,59,55);

HFONT font(int size=14,int weight=FW_NORMAL);
void fill(HDC dc,const RECT& rect,COLORREF color);
void rounded(HDC dc,const RECT& rect,COLORREF color,COLORREF border=line,int radius=8);
bool hovered(HWND control);
void text(HDC dc,RECT rect,std::wstring_view value,COLORREF color=ink,HFONT face=nullptr,
    UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
void darkCaption(HWND window);
void centerDialog(HWND window,HWND owner);
DWORD controlStyle(const wchar_t* type,DWORD style);
void styleControl(HWND control);
HBRUSH controlColors(HDC dc,UINT message);
bool paintDialog(HWND window,UINT message,WPARAM wParam,LPARAM lParam,LRESULT& result,int primary);
void drawButton(const DRAWITEMSTRUCT& item,bool primary=false,bool active=false);
void drawRow(const DRAWITEMSTRUCT& item,std::wstring_view value,COLORREF marker=CLR_INVALID,
    std::wstring_view secondary={});
}
