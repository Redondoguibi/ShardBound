#pragma once
#include <windows.h>
#include <string_view>

namespace game::editor {
inline constexpr auto background=RGB(17,20,25);
inline constexpr auto panel=RGB(25,29,37);
inline constexpr auto field=RGB(32,38,48);
inline constexpr auto line=RGB(48,56,69);
inline constexpr auto canvas=RGB(19,23,30);
inline constexpr auto ink=RGB(234,238,244);
inline constexpr auto muted=RGB(155,168,185);
inline constexpr auto accent=RGB(239,183,114);
inline constexpr auto selected=RGB(62,49,38);

HFONT font(int size=14,int weight=FW_NORMAL);
void fill(HDC dc,const RECT& rect,COLORREF color);
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
