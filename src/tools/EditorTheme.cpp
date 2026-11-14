#include "tools/EditorTheme.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <map>
#include <string>

namespace game::editor {
namespace {
LRESULT CALLBACK comboFrame(HWND window,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR id,DWORD_PTR) {
    if(message==WM_NCDESTROY) RemoveWindowSubclass(window,comboFrame,id);
    const auto result=DefSubclassProc(window,message,wParam,lParam);
    if(message==WM_PAINT && IsWindowVisible(window)) {
        COMBOBOXINFO info{sizeof(info)};
        if(GetComboBoxInfo(window,&info)) {
            const auto dc=GetDC(window);
            auto arrow=info.rcButton;
            fill(dc,arrow,field);
            const LONG x=(arrow.left+arrow.right)/2,y=(arrow.top+arrow.bottom)/2;
            const POINT glyph[]={{x-4,y-2},{x+4,y-2},{x,y+3}};
            const auto brush=SelectObject(dc,GetStockObject(DC_BRUSH));
            const auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
            SetDCBrushColor(dc,muted);Polygon(dc,glyph,3);
            SelectObject(dc,pen);SelectObject(dc,brush);
            RECT frame{};GetClientRect(window,&frame);
            SetDCBrushColor(dc,GetFocus()==window?accent:line);FrameRect(dc,&frame,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            ReleaseDC(window,dc);
        }
    }
    return result;
}
}
HFONT font(int size,int weight) {
    struct Fonts {
        std::map<std::pair<int,int>,HFONT> values;
        ~Fonts() {for(const auto& [key,font]:values) { (void)key;DeleteObject(font); }}
    };
    static Fonts fonts;
    const auto [it,added]=fonts.values.try_emplace({size,weight},nullptr);
    if(added) it->second=CreateFontW(-size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    return it->second;
}

void fill(HDC dc,const RECT& rect,COLORREF color) {
    SetDCBrushColor(dc,color);
    FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void text(HDC dc,RECT rect,std::wstring_view value,COLORREF color,HFONT face,UINT flags) {
    const auto previous=SelectObject(dc,face?face:font());
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,color);
    DrawTextW(dc,value.data(),int(value.size()),&rect,flags);
    SelectObject(dc,previous);
}

void darkCaption(HWND window) {
    const BOOL dark=TRUE;
    DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
}

void centerDialog(HWND window,HWND owner) {
    RECT frame{},parent{};
    GetWindowRect(window,&frame);GetWindowRect(owner,&parent);
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor)) return;
    const auto width=frame.right-frame.left,height=frame.bottom-frame.top;
    const auto& work=monitor.rcWork;
    const auto x=std::clamp((parent.left+parent.right-width)/2,work.left,std::max(work.left,work.right-width));
    const auto y=std::clamp((parent.top+parent.bottom-height)/2,work.top,std::max(work.top,work.bottom-height));
    SetWindowPos(window,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}

DWORD controlStyle(const wchar_t* type,DWORD style) {
    if(std::wstring_view(type)==L"BUTTON" && (style&BS_TYPEMASK)!=BS_AUTOCHECKBOX)
        return (style&~BS_TYPEMASK)|BS_OWNERDRAW;
    if(std::wstring_view(type)==L"COMBOBOX") return style|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
    return style;
}

void styleControl(HWND control) {
    SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font()),FALSE);
    SetWindowTheme(control,L"DarkMode_Explorer",nullptr);
    wchar_t type[32]{};
    GetClassNameW(control,type,int(std::size(type)));
    if(std::wstring_view(type)==L"ComboBox") {
        SetWindowSubclass(control,comboFrame,1,0);
        SendMessageW(control,CB_SETITEMHEIGHT,WPARAM(-1),25);
    }
}

HBRUSH controlColors(HDC dc,UINT message) {
    static const auto panelBrush=CreateSolidBrush(panel);
    static const auto fieldBrush=CreateSolidBrush(field);
    SetTextColor(dc,ink);
    if(message==WM_CTLCOLORSTATIC) {
        SetBkColor(dc,panel);SetBkMode(dc,TRANSPARENT);
        return panelBrush;
    }
    SetBkColor(dc,field);SetBkMode(dc,OPAQUE);
    return fieldBrush;
}

bool paintDialog(HWND window,UINT message,WPARAM wParam,LPARAM lParam,LRESULT& result,int primary) {
    if(message==WM_ERASEBKGND) {
        RECT area{};GetClientRect(window,&area);
        fill(reinterpret_cast<HDC>(wParam),area,panel);result=1;
    } else if(message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX || message==WM_CTLCOLORSTATIC || message==WM_CTLCOLORBTN) {
        result=reinterpret_cast<LRESULT>(controlColors(reinterpret_cast<HDC>(wParam),message));
    } else if(message==WM_MEASUREITEM) {
        const auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if(item->CtlType!=ODT_COMBOBOX) return false;
        item->itemHeight=28;result=TRUE;
    } else if(message==WM_DRAWITEM) {
        const auto& item=*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if(item.CtlType==ODT_BUTTON) drawButton(item,item.CtlID==UINT(primary));
        else if(item.CtlType==ODT_COMBOBOX) {
            const auto length=SendMessageW(item.hwndItem,CB_GETLBTEXTLEN,item.itemID,0);
            if(length<0) fill(item.hDC,item.rcItem,field);
            else {
                std::wstring label(size_t(length)+1,L'\0');
                SendMessageW(item.hwndItem,CB_GETLBTEXT,item.itemID,reinterpret_cast<LPARAM>(label.data()));
                label.resize(size_t(length));drawRow(item,label);
            }
        } else return false;
        result=TRUE;
    } else return false;
    return true;
}

void drawButton(const DRAWITEMSTRUCT& item,bool primary,bool active) {
    wchar_t label[160]{};
    GetWindowTextW(item.hwndItem,label,int(std::size(label)));
    auto bounds=item.rcItem;
    const bool enabled=!(item.itemState&ODS_DISABLED);
    const bool pressed=item.itemState&ODS_SELECTED;
    const auto bg=primary?(pressed?RGB(212,152,86):accent):active?selected:pressed?line:field;
    fill(item.hDC,bounds,primary && !enabled?field:bg);
    SetDCBrushColor(item.hDC,active?accent:primary?bg:line);
    FrameRect(item.hDC,&bounds,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    InflateRect(&bounds,-5,-3);
    text(item.hDC,bounds,label,enabled?(primary?background:active?accent:ink):muted,font(14,FW_SEMIBOLD),
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    if(item.itemState&ODS_FOCUS) DrawFocusRect(item.hDC,&bounds);
}

void drawRow(const DRAWITEMSTRUCT& item,std::wstring_view value,COLORREF marker,std::wstring_view secondary) {
    auto bounds=item.rcItem;
    const bool active=item.itemState&ODS_SELECTED;
    fill(item.hDC,bounds,active?selected:field);
    bounds.left+=10;
    bounds.right-=8;
    if(active) fill(item.hDC,{item.rcItem.left,item.rcItem.top,item.rcItem.left+2,item.rcItem.bottom},accent);
    if(marker!=CLR_INVALID) {
        const auto y=(bounds.top+bounds.bottom)/2;
        fill(item.hDC,{bounds.left,y-7,bounds.left+14,y+7},marker);
        bounds.left+=23;
    }
    if(!secondary.empty()) {
        auto title=bounds;title.bottom=title.top+23;
        text(item.hDC,title,value,ink,font(14,FW_SEMIBOLD));
        bounds.top=title.bottom-3;
        text(item.hDC,bounds,secondary,muted,font(12));
    } else text(item.hDC,bounds,value,active?accent:ink);
    if(item.itemState&ODS_FOCUS) DrawFocusRect(item.hDC,&item.rcItem);
}
}
