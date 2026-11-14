#include "tools/BlockDialog.h"
#include "tools/EditorTheme.h"
#include "assets/AssetId.h"
#include <commdlg.h>
#include <windowsx.h>

namespace game {
namespace {
constexpr int grid=23,left=24,top=109;
enum { Mod=1,Solid,Color,Publish,Cancel,Cube,Model,ImportModel,ModelFile };

class BlockDialog {
public:
    BlockDialog() { block_.pixels.fill(0x848f7d); }
    std::optional<AuthoredBlock> run(HWND parent) {
        WNDCLASSW type{};type.lpfnWndProc=dispatch;type.hInstance=GetModuleHandleW(nullptr);
        type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        type.lpszClassName=L"ShardBoundBlockDialog";
        if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register pixel editor");
        const auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"Novo bloco",WS_CAPTION|WS_SYSMENU|WS_POPUP,
            CW_USEDEFAULT,CW_USEDEFAULT,755,593,parent,nullptr,type.hInstance,this);
        if(!window) throw std::runtime_error("Cannot open pixel editor");
        editor::darkCaption(window);editor::centerDialog(window,parent);
        EnableWindow(parent,FALSE);ShowWindow(window,SW_SHOW);SetFocus(mod_);
        MSG message{};BOOL result=TRUE;
        while(!done_ && (result=GetMessageW(&message,nullptr,0,0))>0) {
            if(!IsDialogMessageW(window,&message)) {TranslateMessage(&message);DispatchMessageW(&message);}
        }
        EnableWindow(parent,TRUE);if(IsWindow(window)) DestroyWindow(window);SetActiveWindow(parent);
        if(result==0) PostQuitMessage(int(message.wParam));
        return accepted_?std::optional{std::move(block_)}:std::nullopt;
    }

private:
    AuthoredBlock block_;
    HWND mod_{},solid_{},window_{};
    bool done_=false,accepted_=false;
    bool modeled_=false;
    COLORREF brush_=RGB(55,125,82);

    HWND child(const wchar_t* type,const wchar_t* title,DWORD style,int x,int y,int w,int h,int id=0) {
        const auto control=CreateWindowExW(0,type,title,WS_CHILD|WS_VISIBLE|editor::controlStyle(type,style),x,y,w,h,window_,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        editor::styleControl(control);
        return control;
    }
    LRESULT handle(UINT message,WPARAM wParam,LPARAM lParam) {
        LRESULT themed=0;
        if(message==WM_DRAWITEM) {
            const auto& item=*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if(item.CtlID==Cube || item.CtlID==Model) {
                editor::drawButton(item,false,(item.CtlID==Model)==modeled_);return TRUE;
            }
        }
        if(editor::paintDialog(window_,message,wParam,lParam,themed,Publish)) return themed;
        switch(message) {
        case WM_CREATE:
            child(L"BUTTON",L"Cubo pintado",WS_TABSTOP,24,59,125,32,Cube);
            child(L"BUTTON",L"Modelo GLB",WS_TABSTOP,158,59,125,32,Model);
            child(L"STATIC",L"ID do mod",0,427,144,215,24);
            mod_=child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,427,171,278,32,Mod);
            SendMessageW(mod_,EM_SETLIMITTEXT,48,0);
            solid_=child(L"BUTTON",L"Colisão sólida (cubo inteiro)",BS_AUTOCHECKBOX|WS_TABSTOP,427,221,275,28,Solid);
            SendMessageW(solid_,BM_SETCHECK,BST_CHECKED,0);
            child(L"BUTTON",L"Cor do pincel…",WS_TABSTOP,427,276,145,32,Color);
            child(L"BUTTON",L"Importar modelo…",WS_TABSTOP,427,276,152,32,ImportModel);
            child(L"STATIC",L"Nenhum modelo selecionado",SS_PATHELLIPSIS,427,319,270,25,ModelFile);
            child(L"BUTTON",L"Publicar bloco",WS_TABSTOP,569,492,136,35,Publish);
            child(L"BUTTON",L"Cancelar",WS_TABSTOP,453,492,104,35,Cancel);
            setMode(false);
            return 0;
        case WM_LBUTTONDOWN: SetCapture(window_);paint(lParam);return 0;
        case WM_LBUTTONUP: if(GetCapture()==window_) ReleaseCapture();return 0;
        case WM_MOUSEMOVE: if(wParam&MK_LBUTTON) paint(lParam);return 0;
        case WM_COMMAND:
            if(LOWORD(wParam)==Cube || LOWORD(wParam)==Model) setMode(LOWORD(wParam)==Model);
            if(LOWORD(wParam)==ImportModel) {
                wchar_t selected[32768]{};
                OPENFILENAMEW file{};file.lStructSize=sizeof(file);file.hwndOwner=window_;
                file.lpstrFilter=L"Modelo GLB\0*.glb\0";file.lpstrFile=selected;file.nMaxFile=DWORD(std::size(selected));
                file.Flags=OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
                if(GetOpenFileNameW(&file)) {
                    block_.model=selected;
                    SetWindowTextW(GetDlgItem(window_,ModelFile),block_.model.filename().c_str());
                }
            }
            if(LOWORD(wParam)==Color) {
                COLORREF custom[16]{};
                CHOOSECOLORW picker{};picker.lStructSize=sizeof(picker);picker.hwndOwner=window_;
                picker.lpCustColors=custom;picker.rgbResult=brush_;picker.Flags=CC_FULLOPEN|CC_RGBINIT;
                if(ChooseColorW(&picker)) brush_=picker.rgbResult;
            }
            if(LOWORD(wParam)==Publish) {
                char id[49]{};GetWindowTextA(mod_,id,49);
                if(!validNamespace(id) || std::string_view(id)=="core") {
                    MessageBoxW(window_,L"Use letras minúsculas, números, hífen ou sublinhado.",L"ID do mod",MB_ICONWARNING);return 0;
                }
                block_.mod=id;block_.solid=SendMessageW(solid_,BM_GETCHECK,0,0)==BST_CHECKED;
                if(modeled_ && block_.model.empty()) {
                    MessageBoxW(window_,L"Importe um modelo GLB para publicar.",L"Modelo do bloco",MB_ICONWARNING);return 0;
                }
                if(!modeled_) block_.model.clear();
                accepted_=done_=true;
            }
            if(LOWORD(wParam)==Cancel) done_=true;
            return 0;
        case WM_PAINT: draw();return 0;
        case WM_CLOSE: done_=true;return 0;
        default: return DefWindowProcW(window_,message,wParam,lParam);
        }
    }
    void setMode(bool model) {
        modeled_=model;
        ShowWindow(GetDlgItem(window_,Color),model?SW_HIDE:SW_SHOWNA);
        ShowWindow(GetDlgItem(window_,ImportModel),model?SW_SHOWNA:SW_HIDE);
        ShowWindow(GetDlgItem(window_,ModelFile),model?SW_SHOWNA:SW_HIDE);
        RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    }
    void paint(LPARAM point) {
        if(modeled_) return;
        const int px=GET_X_LPARAM(point)-left,py=GET_Y_LPARAM(point)-top;
        if(px<0 || py<0 || px>=grid*16 || py>=grid*16) return;
        block_.pixels[size_t(py/grid)*16+px/grid]=std::uint32_t(GetRValue(brush_))<<16|
            std::uint32_t(GetGValue(brush_))<<8|GetBValue(brush_);
        const RECT area{left,top,left+grid*16,top+grid*16};InvalidateRect(window_,&area,FALSE);
    }
    void draw() const {
        PAINTSTRUCT paint{};const auto dc=BeginPaint(window_,&paint);
        editor::text(dc,{24,12,704,49},L"Novo bloco",editor::ink,editor::font(26,FW_SEMIBOLD));
        editor::text(dc,{427,109,705,141},L"Propriedades",editor::ink,editor::font(19,FW_SEMIBOLD));
        if(modeled_) {
            editor::fill(dc,{left,top,left+grid*16,top+grid*16},editor::canvas);
            editor::text(dc,{left+24,top+26,left+grid*16-24,top+64},L"Geometria personalizada",editor::ink,editor::font(20,FW_SEMIBOLD));
            editor::text(dc,{left+24,top+76,left+grid*16-24,top+260},
                L"Modele no Blockbench e exporte como GLB. Posicione a base em Y = 0 e centralize o modelo em X e Z, dentro de uma célula. Depois de publicar, coloque o bloco no mapa para conferir em 3D.",
                editor::muted,editor::font(15),DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
            EndPaint(window_,&paint);return;
        }
        const auto gridBrush=static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH));
        for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
            const auto rgb=block_.pixels[size_t(y)*16+x];const auto brush=CreateSolidBrush(RGB(rgb>>16,rgb>>8,rgb));
            const RECT cell{left+x*grid,top+y*grid,left+(x+1)*grid,top+(y+1)*grid};
            FillRect(dc,&cell,brush);FrameRect(dc,&cell,gridBrush);DeleteObject(brush);
        }
        EndPaint(window_,&paint);
    }
    static LRESULT CALLBACK dispatch(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        if(message==WM_NCCREATE) {
            const auto create=reinterpret_cast<const CREATESTRUCTW*>(lParam);
            const auto self=static_cast<BlockDialog*>(create->lpCreateParams);self->window_=window;
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        const auto self=reinterpret_cast<BlockDialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        return self?self->handle(message,wParam,lParam):DefWindowProcW(window,message,wParam,lParam);
    }
};
}

std::optional<AuthoredBlock> editNewBlock(HWND parent) { return BlockDialog{}.run(parent); }
}
