#include "world/FiniteMap.h"
#include "world/ChunkWorld.h"
#include "world/MapImport.h"
#include "world/WorldDefinition.h"
#include "tools/BlockDialog.h"
#include "tools/BlockbenchPane.h"
#include "tools/EquipmentDialog.h"
#include "tools/AiDialog.h"
#include "tools/EditorProcess.h"
#include "tools/EditorWorkspace.h"
#include "tools/EditorTimeline.h"
#include "tools/EditorTheme.h"
#include "tools/MapPackage.h"
#include "assets/AssetManager.h"
#include "assets/StudioPackage.h"
#include "assets/VirtualFileSystem.h"
#include "core/AtomicFile.h"
#include "core/PathText.h"
#include "platform/UserPaths.h"
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <windowsx.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>

namespace {
using game::FiniteMap;
using game::WorldDefinition;
namespace theme=game::editor;
constexpr int inspector=294, toolbar=66, footer=30, gutter=18;
constexpr int tabSelectBase=3000,tabCloseBase=3100,tabLimit=32;
constexpr UINT_PTR blockbenchTestTimer=42;
enum Control { NewMap=101, OpenMap, SaveMap, ImportSave, LayerDown, LayerUp, Undo, Redo, TestMap, OpenBlockbench, Palette, CropX, CropZ, CropWidth, CropDepth, Filter, Structure, Rotation, Timeline, LinkRoom, NewBlock, Equipment, View3D, Props, PropScale, Ai, ExportMap, RegisterStructure, SyncStudio, ExportContent };
enum LayoutControl { View2D=201, ProjectMenu, ApplyRegion, CancelRegion, SearchLabel,
    BrushTool, EraseTool, PickTool, FillTool, ResetCamera, Settings,
    LibraryPanel, PlacementPanel, HistoryPanel, HomeNew, HomeOpen, HomeMap, HomeBlocks, HomeGear, HomeAi,
    BlocksCreate, GearCreate, AiStart, Start3D, GridToggle, CameraSpeed, ChooseBlockbench, LaunchBlockbench, ChooseBridge,
    OpenBlockbenchFile, BlockbenchTab, ReloadBlockbench, NewTab, PreviousTabs, NextTabs, CloseTab, CycleTabs, ReverseTabs,
    BlockName, BlockSolid, BlockSave, BlockReturn, BlockEdit, BlockImport };
enum class TerrainTool { Brush, Erase, Pick, Fill };
enum class Page {Home,Map,Blocks,Gear,Ai,Settings,Blockbench,BlockModel};
enum class MapPanel {Library,Placement,History};

std::wstring wide(std::string_view text) {
    const int count=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);
    std::wstring result(size_t(count),L'\0');
    if(count) MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),count);
    return result;
}

std::string utf8(std::wstring_view text) {
    const int count=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(size_t(count),'\0');
    if(count) WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),count,nullptr,nullptr);
    return result;
}

std::wstring assetName(std::string_view id) {
    static const std::map<std::string_view,std::wstring_view> names{
        {"plains:grass",L"Grama"},{"plains:dirt",L"Terra"},{"plains:stone",L"Pedra"},
        {"plains:brick",L"Tijolos"},{"plains:wood",L"Madeira"},{"plains:leaves",L"Folhas"},
        {"plains:flower",L"Flor"},{"plains:oak",L"Carvalho"}};
    if(const auto known=names.find(id);known!=names.end()) return std::wstring(known->second);
    auto label=wide(id.substr(id.find(':')+1));
    std::replace(label.begin(),label.end(),L'_',L' ');
    if(!label.empty()) label.front()=wchar_t(towupper(label.front()));
    return label;
}

COLORREF blockColor(std::string_view id) {
    static const std::map<std::string_view,COLORREF> colors{
        {"plains:grass",RGB(104,146,86)},{"plains:dirt",RGB(139,104,73)},{"plains:stone",RGB(132,139,145)},
        {"plains:brick",RGB(183,113,83)},{"plains:wood",RGB(151,110,69)},{"plains:leaves",RGB(69,111,73)},
        {"plains:flower",RGB(221,153,157)}};
    if(const auto known=colors.find(id);known!=colors.end()) return known->second;
    const auto hash=game::seedHash(id);
    return RGB(74+(hash&120),86+((hash>>8)&108),83+((hash>>16)&110));
}

class Editor {
public:
    bool blockbenchInput(HWND target) const {
        return (blockbenchPane_ && blockbenchPane_->owns(target)) ||
            (currentTab_ && currentTab_->block && currentTab_->block->pane->owns(target));
    }
    Editor(WorldDefinition catalog,std::filesystem::path programDirectory,std::filesystem::path mods,bool smoke,bool blockbenchTest)
        : catalog_(std::move(catalog)),programDirectory_(std::move(programDirectory)),mods_(std::move(mods)),smoke_(smoke),
          blockbenchTest_(blockbenchTest) {
        if(!catalog_.maps.empty()) map_=*catalog_.maps.begin()->second;
        for(const auto& [id,block]:catalog_.blocks) {
            (void)block;
            colors_.emplace(id,blockColor(id));
            ids_.push_back(id);
        }
    }

    LRESULT handle(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        switch(message) {
        case WM_CREATE: createControls(window);return 0;
        case WM_SIZE: if(wParam!=SIZE_MINIMIZED) layout();return 0;
        case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize={1040,690};return 0;
        case WM_ERASEBKGND: return 1;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
            return reinterpret_cast<LRESULT>(theme::controlColors(reinterpret_cast<HDC>(wParam),message));
        case WM_MEASUREITEM: {
            const auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            item->itemHeight=item->CtlID==Palette?40:29;
            return TRUE;
        }
        case WM_DRAWITEM: drawControl(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));return TRUE;
        case WM_MOUSEWHEEL: {
            POINT cursor{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};ScreenToClient(window_,&cursor);
            const auto area=previewViewport();
            if(PtInRect(&area,cursor)) zoom(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        }
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
            beginCameraGesture(message==WM_LBUTTONDOWN,{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)});
            return 0;
        case WM_MOUSEMOVE:
            if(cameraGesture_) updateCameraGesture({GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)});
            return 0;
        case WM_LBUTTONUP: case WM_RBUTTONUP:
            finishCameraGesture(message==WM_LBUTTONUP,{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)});
            return 0;
        case WM_CANCELMODE:
            cancelCameraGesture();
            return 0;
        case WM_CAPTURECHANGED:
            cameraGesture_.reset();
            return 0;
        case WM_COMMAND:
            if(LOWORD(wParam)==BlockName) {
                if(HIWORD(wParam)==EN_CHANGE && currentTab_ && currentTab_->block && !settingBlockFields_) {
                    currentTab_->block->name=controlText(GetDlgItem(window_,BlockName));
                    currentTab_->name=blockTabName(*currentTab_->block);
                    markBlockChanged(*currentTab_);
                }
                return 0;
            }
            if(LOWORD(wParam)==Filter) {
                if(HIWORD(wParam)==EN_CHANGE) {filterPalette();InvalidateRect(window_,nullptr,FALSE);}
                return 0;
            }
            if(LOWORD(wParam)==Palette) {
                if(HIWORD(wParam)==LBN_SELCHANGE) {
                    selectTerrainTool(TerrainTool::Brush);
                }
                if(HIWORD(wParam)==LBN_DBLCLK) editSelectedBlock();
                return 0;
            }
            if(LOWORD(wParam)==Timeline) { if(HIWORD(wParam)==LBN_SELCHANGE) jumpToEdit();return 0; }
            command(LOWORD(wParam));return 0;
        case WM_KEYDOWN:
            if(page_==Page::Map && view3d_) {
                if(wParam==VK_LEFT || wParam=='A') panCamera(-25,0);
                if(wParam==VK_RIGHT || wParam=='D') panCamera(25,0);
                if(wParam==VK_UP || wParam=='W') panCamera(0,25);
                if(wParam==VK_DOWN || wParam=='S') panCamera(0,-25);
                if(wParam=='Q') orbit_-=.20f;
                if(wParam=='E') orbit_+=.20f;
                refresh();
            }
            return 0;
        case WM_PAINT: draw();return 0;
        case WM_CLOSE: if(confirmAllTabs()) DestroyWindow(window);return 0;
        case WM_APP+2: moveTab(int(wParam),{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)});return 0;
        case WM_APP+3:
            SetTimer(window_,blockbenchTestTimer,45000,nullptr);
            command(NewBlock);return 0;
        case WM_APP+4: verifyBlockbenchResult();return 0;
        case WM_TIMER:
            if(wParam==blockbenchTestTimer) {
                KillTimer(window_,blockbenchTestTimer);
                const auto status=currentTab_ && currentTab_->block?currentTab_->block->status:L"Sem aba do Blockbench";
                std::fprintf(stderr,"Blockbench test timed out: %s\n",utf8(status).c_str());
                DestroyWindow(window);PostQuitMessage(1);
            }
            return 0;
        case WM_APP+1: {
            if(page_!=Page::Home || !(GetWindowLongPtrW(GetDlgItem(window_,HomeNew),GWL_STYLE)&WS_VISIBLE))
                throw std::runtime_error("Home view did not open first");
            go(Page::Map);
            command(NewMap);
            if(!(GetWindowLongPtrW(crop_[0],GWL_STYLE)&WS_VISIBLE)) throw std::runtime_error("Map region was not shown");
            command(CancelRegion);
            if(GetWindowLongPtrW(crop_[0],GWL_STYLE)&WS_VISIBLE) throw std::runtime_error("Map region stayed visible after canceling");
            SetWindowTextW(filter_,L"GrAmA");
            if(visibleIds_.size()!=1 || ids_[visibleIds_.front()]!="plains:grass") throw std::runtime_error("Named block filtering failed");
            SetWindowTextW(filter_,L"");
            command(PlacementPanel);
            if(GetWindowLongPtrW(palette_,GWL_STYLE)&WS_VISIBLE) throw std::runtime_error("Map panels did not switch");
            command(LibraryPanel);
            layer_=20;
            command(FillTool);
            const auto bounds=map_.bounds();
            const auto area=viewport();
            const auto click=[&](int x,int z) {
                const auto point=MAKELPARAM(area.left+(2*x+1)*(area.right-area.left)/(2*bounds.width),
                    area.top+(2*z+1)*(area.bottom-area.top)/(2*bounds.depth));
                SendMessageW(window_,WM_LBUTTONDOWN,MK_LBUTTON,point);
                SendMessageW(window_,WM_LBUTTONUP,0,point);
            };
            click(0,0);click(1,1);
            if(history_.size()!=1 || history_.front().cells.empty()) throw std::runtime_error("Palette selection could not paint a region");
            command(EraseTool);click(0,0);
            if(history_.size()!=2 || !map_.block(bounds.minX,layer_,bounds.minZ).empty())
                throw std::runtime_error("The erase tool no longer works with a left click");
            command(Undo);
            command(Undo);
            replace(map_,{},false);layer_=3;command(BrushTool);
            zoom(WHEEL_DELTA);
            if(plantScale_<=1) throw std::runtime_error("Plan zoom did not respond");
            togglePreview();drawPreview();
            if(previewRenderer_->stats().triangles==0) throw std::runtime_error("3D editor did not render terrain");
            verifyCameraGestures();
            verifyTimeline();
            verifyTabs();
            DestroyWindow(window);return 0;
        }
        case WM_DESTROY:
            blockbenchPane_.reset();
            for(auto& tab:tabs_) tab->block.reset();
            previewMeshes_.clear();previewAssets_.reset();previewRenderer_.reset();PostQuitMessage(0);return 0;
        default: return DefWindowProcW(window,message,wParam,lParam);
        }
    }

private:
    using Edit=game::EditorEdit;
    HWND window_{};
    HWND palette_{};
    HWND filter_{};
    std::wstring status_;
    HWND structures_{},rotation_{},timeline_{};
    HWND props_{},propScale_{};
    std::array<HWND,4> crop_{};
    WorldDefinition catalog_;
    std::filesystem::path programDirectory_,mods_;
    std::filesystem::path defaultMods_=mods_;
    FiniteMap map_{{0,0,64,64}};
    std::filesystem::path path_;
    std::vector<std::string> ids_;
    std::vector<size_t> visibleIds_;
    std::vector<std::string> structureIds_;
    std::vector<std::string> modelIds_;
    std::map<std::string,COLORREF,std::less<>> colors_;
    std::vector<Edit> history_;
    size_t cursor_=0,saved_=0;
    bool needsSave_=false;
    int layer_=3;
    std::shared_ptr<const FiniteMap> roomToLink_;
    std::optional<FiniteMap::Spawn> entranceToLink_;
    HWND renderSurface_{};
    std::unique_ptr<game::Renderer> previewRenderer_;
    game::VirtualFileSystem previewFiles_;
    std::unique_ptr<game::AssetManager> previewAssets_;
    bool previewCatalogDirty_=true;
    std::vector<game::RenderItem> previewMeshes_;
    bool view3d_=false,previewDirty_=true;
    bool smoke_=false,blockbenchTest_=false;
    unsigned testBlockStage_=0;
    std::filesystem::path testMapPath_;
    float centerX_=0,centerZ_=0,orbit_=.72f,distance_=75;
    std::filesystem::path blockbench_;
    std::optional<FiniteMap::Spawn> structureCorner_;
    bool selectingStructure_=false;
    int structureLayer_=0;
    int regionCommand_=0;
    bool ready_=false;
    struct CameraGesture {
        POINT start,last;
        bool left,moved=false;
    };
    std::optional<CameraGesture> cameraGesture_;
    float pitch_=.88f,plantScale_=1,plantX_=0,plantY_=0;
    bool showGrid_=true;
    float cameraSpeed_=1;
    bool start3d_=false;
    Page page_=Page::Home;
    MapPanel panel_=MapPanel::Library;
    std::filesystem::path bridge_;
    std::unique_ptr<game::editor::BlockbenchPane> blockbenchPane_;
    TerrainTool terrainTool_=TerrainTool::Brush;
    std::optional<FiniteMap::Spawn> fillCorner_;
    struct MapSession {
        FiniteMap map;
        std::filesystem::path path,mods;
        std::vector<Edit> history;
        size_t cursor,saved;
        bool dirty,view3d;
        int layer;
        MapPanel panel;
        TerrainTool tool;
        std::wstring filter,propScale;
        std::string block;
        std::string structure,model;
        int rotation=0;
        float x,z,orbit,distance,pitch,zoom,panX,panY;
    };
    struct Tab;
    struct BlockDraft {
        Tab* map{};
        std::unique_ptr<game::editor::BlockbenchPane> pane;
        std::wstring name;
        std::wstring status=L"Carregando o modelo de bloco…";
        std::string id,source;
        bool solid=true,ready=false,publishing=false,changed=false;
        unsigned changes=0,savingChanges=0;
        unsigned ticket=0;
    };
    struct Tab {
        Page page;
        std::wstring name;
        HWND select{},close{};
        int slot;
        std::optional<MapSession> map;
        std::unique_ptr<BlockDraft> block;
    };
    std::vector<std::unique_ptr<Tab>> tabs_;
    Tab* currentTab_=nullptr;
    Tab* loadedMap_=nullptr;
    size_t firstTab_=0;
    unsigned nextMap_=1;
    unsigned nextBlock_=1,nextPublication_=0;
    bool settingBlockFields_=false;
    static constexpr int tabsLeft=164,tabsTop=20,tabHeight=39;
    static constexpr int tabGap=2,minTabWidth=118,tabActionsWidth=120;
    HWND hoveredTab_{};
    POINT tabDown_{};
    bool movingTab_=false;

    HWND child(const wchar_t* type,const wchar_t* title,DWORD style,int x,int y,int w,int h,int id=0) {
        if(std::wstring_view(type)==L"BUTTON") style|=BS_OWNERDRAW;
        const auto control=CreateWindowExW(0,type,title,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window_,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(!control) throw std::runtime_error("Cannot create editor control");
        theme::styleControl(control);
        return control;
    }

    Tab& addTab(Page page,std::wstring name) {
        if(tabs_.size()>=tabLimit) throw std::runtime_error("Feche uma aba antes de abrir outra.");
        int slot=0;
        while(std::any_of(tabs_.begin(),tabs_.end(),[slot](const auto& t) {return t->slot==slot;})) ++slot;
        auto tab=std::make_unique<Tab>(Tab{page,std::move(name),nullptr,nullptr,slot,{},nullptr});
        tab->select=child(L"BUTTON",tab->name.c_str(),WS_TABSTOP,0,0,0,0,tabSelectBase+slot);
        if(!tabs_.empty()) {
            try {
                tab->close=child(L"BUTTON",L"Fechar aba",WS_TABSTOP,0,0,0,0,tabCloseBase+slot);
            } catch(...) {
                DestroyWindow(tab->select);
                throw;
            }
        }
        SetWindowSubclass(tab->select,tabInput,1,reinterpret_cast<DWORD_PTR>(this));
        if(tab->close) SetWindowSubclass(tab->close,tabInput,1,reinterpret_cast<DWORD_PTR>(this));
        tabs_.push_back(std::move(tab));
        return *tabs_.back();
    }

    static LRESULT CALLBACK tabInput(HWND control,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR subclass,DWORD_PTR data) {
        auto& editor=*reinterpret_cast<Editor*>(data);
        const int id=GetDlgCtrlID(control);
        const bool draggable=id>tabSelectBase && id<tabSelectBase+tabLimit;
        const POINT point{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
        switch(message) {
        case WM_MOUSEMOVE:
            if(editor.hoveredTab_!=control) {
                editor.hoveredTab_=control;
                TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,control,0};
                TrackMouseEvent(&tracking);
                InvalidateRect(control,nullptr,FALSE);
            }
            if(draggable && GetCapture()==control && (wParam&MK_LBUTTON)) {
                if(std::abs(point.x-editor.tabDown_.x)>=GetSystemMetrics(SM_CXDRAG) ||
                    std::abs(point.y-editor.tabDown_.y)>=GetSystemMetrics(SM_CYDRAG)) editor.movingTab_=true;
                if(editor.movingTab_) SetCursor(LoadCursorW(nullptr,IDC_SIZEWE));
            }
            break;
        case WM_MOUSELEAVE:
            if(editor.hoveredTab_==control) editor.hoveredTab_=nullptr;
            InvalidateRect(control,nullptr,FALSE);
            break;
        case WM_LBUTTONDOWN:
            if(draggable) {
                editor.tabDown_=point;editor.movingTab_=false;
                SetFocus(control);SetCapture(control);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if(draggable && GetCapture()==control) {
                const bool moving=editor.movingTab_;
                ReleaseCapture();
                if(moving) {
                    POINT target=point;
                    MapWindowPoints(control,editor.window_,&target,1);
                    SendMessageW(editor.window_,WM_APP+2,id-tabSelectBase,MAKELPARAM(target.x,target.y));
                } else {
                    RECT bounds{};GetClientRect(control,&bounds);
                    if(PtInRect(&bounds,point)) SendMessageW(editor.window_,WM_COMMAND,id,0);
                }
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if(draggable) editor.movingTab_=false;
            break;
        case WM_MBUTTONUP:
            if(draggable) SendMessageW(editor.window_,WM_COMMAND,tabCloseBase+id-tabSelectBase,0);
            return 0;
        case WM_NCDESTROY:
            if(editor.hoveredTab_==control) editor.hoveredTab_=nullptr;
            RemoveWindowSubclass(control,tabInput,subclass);
            break;
        }
        return DefSubclassProc(control,message,wParam,lParam);
    }

    Tab* findTab(Page page) const {
        for(const auto& tab:tabs_) if(tab->page==page) return tab.get();
        return nullptr;
    }

    size_t tabIndex(const Tab& tab) const {
        return std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& item) {return item.get()==&tab;})-tabs_.begin();
    }

    bool tabDirty(const Tab& tab) const {
        if(tab.block) return tab.block->changed;
        return tab.page==Page::Map && (&tab==loadedMap_?dirty():tab.map && tab.map->dirty);
    }

    std::wstring tabLabel(const Tab& tab) const {
        if(tab.page!=Page::Map) return tab.name;
        if(&tab==loadedMap_ && !path_.empty()) return path_.stem().wstring();
        if(tab.map && !tab.map->path.empty()) return tab.map->path.stem().wstring();
        return tab.name;
    }

    void updateTabLabels() const {
        for(const auto& tab:tabs_) {
            const auto label=tabLabel(*tab)+(tabDirty(*tab)?L" (alterado)":L"");
            if(controlText(tab->select)!=label) SetWindowTextW(tab->select,label.c_str());
            InvalidateRect(tab->select,nullptr,FALSE);
            if(tab->close) InvalidateRect(tab->close,nullptr,FALSE);
        }
    }

    static std::wstring controlText(HWND control) {
        std::wstring value(size_t(GetWindowTextLengthW(control))+1,L'\0');
        value.resize(size_t(GetWindowTextW(control,value.data(),int(value.size()))));
        return value;
    }

    void drawTab(const DRAWITEMSTRUCT& item) const {
        const bool close=item.CtlID>=tabCloseBase;
        const int slot=int(item.CtlID)-(close?tabCloseBase:tabSelectBase);
        const auto entry=std::find_if(tabs_.begin(),tabs_.end(),[slot](const auto& t) {return t->slot==slot;});
        if(entry==tabs_.end()) return;
        const auto& tab=**entry;
        const bool active=&tab==currentTab_,pressed=(item.itemState&ODS_SELECTED)!=0;
        const bool focused=(item.itemState&(ODS_FOCUS|ODS_NOFOCUSRECT))==ODS_FOCUS;
        const auto box=item.rcItem;
        const auto surface=active || item.hwndItem==hoveredTab_?theme::field:theme::panel;
        theme::fill(item.hDC,box,pressed?theme::line:surface);
        if(active) theme::fill(item.hDC,{box.left,box.bottom-2,box.right,box.bottom},theme::accent);
        if(close) {
            theme::text(item.hDC,box,L"×",active || focused || item.hwndItem==hoveredTab_?theme::ink:theme::muted,
                theme::font(19),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        } else {
            const std::array glyphs{L"⌂",L"▦",L"◇",L"◈",L"✦",L"⚙",L"◆",L"◇"};
            theme::text(item.hDC,{box.left+9,box.top,box.left+31,box.bottom},glyphs[size_t(tab.page)],
                active?theme::accent:theme::muted,theme::font(16,FW_SEMIBOLD));
            const bool changed=tabDirty(tab);
            theme::text(item.hDC,{box.left+32,box.top,box.right-(changed?14:6),box.bottom},tabLabel(tab),
                active?theme::ink:theme::muted,theme::font(13,active?FW_SEMIBOLD:FW_NORMAL));
            if(changed) theme::text(item.hDC,{box.right-12,box.top,box.right,box.bottom},L"•",
                theme::accent,theme::font(14));
        }
        if(focused && IsWindowVisible(item.hwndItem)) {
            RECT frame=box;InflateRect(&frame,-2,-3);DrawFocusRect(item.hDC,&frame);
        }
    }

    static int tabSpace(int right) {
        return std::max(minTabWidth+tabGap,right-tabsLeft-tabActionsWidth);
    }

    size_t visibleTabs(int right) const {
        return std::min(tabs_.size(),size_t(tabSpace(right)/(minTabWidth+tabGap)));
    }

    void layoutTabs(int right) {
        constexpr int maxWidth=172,closeWidth=28;
        const auto visible=visibleTabs(right);
        const bool overflow=visible<tabs_.size();
        firstTab_=std::min(firstTab_,tabs_.size()-visible);
        const int width=std::min(maxWidth,tabSpace(right)/int(visible)-tabGap);
        const auto place=[](HWND control,int x,int y,int w,int h,bool show) {
            if(!control) return;
            if(show) MoveWindow(control,x,y,w,h,FALSE);
            ShowWindow(control,show?SW_SHOWNA:SW_HIDE);
        };
        int x=tabsLeft;
        for(size_t i=0;i<tabs_.size();++i) {
            const auto& tab=*tabs_[i];
            const bool show=i>=firstTab_ && i<firstTab_+visible;
            const int labelWidth=width-(tab.close?closeWidth:0);
            place(tab.select,x,tabsTop,labelWidth,tabHeight,show);
            place(tab.close,x+labelWidth,tabsTop,closeWidth,tabHeight,show);
            if(show) x+=width+tabGap;
        }
        place(GetDlgItem(window_,NewTab),x+5,tabsTop+4,32,31,true);
        place(GetDlgItem(window_,PreviousTabs),x+45,tabsTop+4,28,31,overflow);
        place(GetDlgItem(window_,NextTabs),x+76,tabsTop+4,28,31,overflow);
        EnableWindow(GetDlgItem(window_,PreviousTabs),firstTab_>0);
        EnableWindow(GetDlgItem(window_,NextTabs),firstTab_+visible<tabs_.size());
    }

    void revealTab(const Tab& tab) {
        RECT bounds{};GetClientRect(window_,&bounds);
        const auto visible=visibleTabs(bounds.right);
        const auto index=tabIndex(tab);
        if(index<firstTab_) firstTab_=index;
        else if(index>=firstTab_+visible) firstTab_=index-visible+1;
    }

    void moveTab(int slot,POINT point) {
        if(point.y<tabsTop || point.y>=tabsTop+tabHeight) return;
        const auto source=std::find_if(tabs_.begin(),tabs_.end(),[slot](const auto& tab) {return tab->slot==slot;});
        if(source==tabs_.end() || source==tabs_.begin()) return;
        auto* dragged=source->get();
        auto destination=tabs_.begin()+std::max(size_t(1),firstTab_);
        for(;destination!=tabs_.end();++destination) {
            const auto control=(*destination)->select;
            if(!(GetWindowLongPtrW(control,GWL_STYLE)&WS_VISIBLE)) break;
            RECT bounds{};GetWindowRect(control,&bounds);
            MapWindowPoints(nullptr,window_,reinterpret_cast<POINT*>(&bounds),2);
            if(point.x<(bounds.left+bounds.right)/2) break;
        }
        if(source<destination) std::rotate(source,source+1,destination);
        else if(source>destination) std::rotate(destination,source,source+1);
        activate(*dragged);
    }

    void createControls(HWND window) {
        window_=window;
        theme::darkCaption(window_);
        const auto button=[&](int id,const wchar_t* label) {child(L"BUTTON",label,WS_TABSTOP,0,0,0,0,id);};
        button(ProjectMenu,L"Projeto  ▾");button(SaveMap,L"Salvar");button(Undo,L"Desfazer");button(Redo,L"Refazer");
        button(TestMap,L"▶  Testar no jogo");
        button(NewBlock,L"+ Novo");button(View2D,L"Planta");button(View3D,L"3D");
        button(LayerDown,L"−");button(LayerUp,L"+");
        button(LinkRoom,L"Vincular interior…");button(RegisterStructure,L"Registrar seleção…");
        button(SyncStudio,L"Atualizar no Blockbench");button(ApplyRegion,L"Continuar");button(CancelRegion,L"Cancelar");
        button(BrushTool,L"Pincel");button(EraseTool,L"Apagar");button(PickTool,L"Amostra");button(FillTool,L"Preencher");
        button(ResetCamera,L"Enquadrar");
        button(NewTab,L"+");button(PreviousTabs,L"‹");button(NextTabs,L"›");button(ReloadBlockbench,L"Recarregar");
        button(LibraryPanel,L"Blocos");button(PlacementPanel,L"Colocação");button(HistoryPanel,L"Histórico");
        button(HomeNew,L"Criar mapa");button(HomeOpen,L"Abrir projeto");button(HomeMap,L"Editar mapa de exemplo");
        button(HomeBlocks,L"Blocos");button(HomeGear,L"Equipamentos");button(HomeAi,L"Assistente IA");
        button(BlockbenchTab,L"Blockbench");button(Settings,L"Configurações");
        button(BlocksCreate,L"Criar bloco");button(GearCreate,L"Criar equipamento");button(AiStart,L"Abrir assistente");
        button(Start3D,L"Vista inicial");button(GridToggle,L"Grade");button(CameraSpeed,L"Velocidade da câmera");
        button(ChooseBlockbench,L"Localizar Blockbench Desktop…");button(LaunchBlockbench,L"Modelar no Blockbench");
        button(ChooseBridge,L"Localizar ponte…");button(OpenBlockbenchFile,L"Abrir no Blockbench Desktop…");
        button(BlockSolid,L"Sólido");button(BlockSave,L"Salvar na engine");button(BlockReturn,L"Ver no mapa");
        button(BlockEdit,L"Editar");button(BlockImport,L"Importar GLB…");
        const auto blockName=child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,BlockName);
        SendMessageW(blockName,EM_SETLIMITTEXT,60,0);
        SendMessageW(blockName,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(9,9));
        const auto listStyle=LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_TABSTOP;
        palette_=child(L"LISTBOX",L"",listStyle,0,0,0,0,Palette);
        filter_=child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,Filter);
        const auto searchLabel=child(L"STATIC",L"Buscar bloco",SS_CENTERIMAGE,0,0,0,0,SearchLabel);
        SendMessageW(searchLabel,WM_SETFONT,reinterpret_cast<WPARAM>(theme::font(13)),FALSE);
        SendMessageW(filter_,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(10,10));
        filterPalette();
        for(size_t i=0;i<crop_.size();++i) {
            crop_[i]=child(L"EDIT",i<2?L"0":L"64",WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,CropX+int(i));
            SendMessageW(crop_[i],EM_SETMARGINS,EC_LEFTMARGIN,MAKELPARAM(8,0));
        }
        constexpr auto comboStyle=CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP;
        structures_=child(L"COMBOBOX",L"",comboStyle,0,0,0,190,Structure);
        SendMessageW(structures_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Sem estrutura"));
        for(const auto& [id,definition]:catalog_.structures) {
            (void)definition;structureIds_.push_back(id);
            const auto label=assetName(id);
            SendMessageW(structures_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(structures_,CB_SETCURSEL,0,0);
        rotation_=child(L"COMBOBOX",L"",comboStyle,0,0,0,135,Rotation);
        for(const auto angle:{L"0°",L"90°",L"180°",L"270°"}) SendMessageW(rotation_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(angle));
        SendMessageW(rotation_,CB_SETCURSEL,0,0);
        timeline_=child(L"LISTBOX",L"",listStyle,0,0,0,0,Timeline);
        SendMessageW(timeline_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Início do histórico"));
        props_=child(L"COMBOBOX",L"",comboStyle,0,0,0,190,Props);
        propScale_=child(L"EDIT",L"1",WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,PropScale);
        SendMessageW(propScale_,EM_SETMARGINS,EC_LEFTMARGIN,MAKELPARAM(8,0));
        for(auto combo:{structures_,props_,rotation_}) SendMessageW(combo,CB_SETITEMHEIGHT,WPARAM(-1),27);
        showModels(readModels(game::discoverMods(mods_).mods));
        currentTab_=&addTab(Page::Home,L"Início");
        ready_=true;if(!smoke_) readSettings();
        updateSettingLabels();layout();refresh();
    }

    void drawControl(const DRAWITEMSTRUCT& item) const {
        const auto id=item.CtlID;
        if(item.CtlType==ODT_BUTTON) {
            if((id>=tabSelectBase && id<tabSelectBase+tabLimit) || (id>=tabCloseBase && id<tabCloseBase+tabLimit)) {
                drawTab(item);return;
            }
            const bool painting=structures_ && props_ && SendMessageW(structures_,CB_GETCURSEL,0,0)==0 &&
                SendMessageW(props_,CB_GETCURSEL,0,0)==0;
            const bool terrain=painting && id==UINT(BrushTool)+UINT(terrainTool_);
            const bool pane=id>=LibraryPanel && id<=HistoryPanel && id==UINT(LibraryPanel)+UINT(panel_);
            const bool primary=id==TestMap || id==ApplyRegion || id==HomeNew || id==BlocksCreate ||
                id==GearCreate || id==AiStart || id==BlockSave;
            theme::drawButton(item,primary,pane || terrain || (id==View3D && view3d_) || (id==View2D && !view3d_));
            return;
        }
        if(item.itemID==UINT(-1)) {
            theme::fill(item.hDC,item.rcItem,theme::field);
            return;
        }
        if(id==Palette && item.itemID<visibleIds_.size()) {
            const auto& block=ids_[visibleIds_[item.itemID]];
            theme::drawRow(item,blockNameFor(block),colors_.at(block),wide(block));
            return;
        }
        const auto isCombo=item.CtlType==ODT_COMBOBOX;
        const auto length=SendMessageW(item.hwndItem,isCombo?CB_GETLBTEXTLEN:LB_GETTEXTLEN,item.itemID,0);
        if(length<0) return;
        std::wstring label(size_t(length)+1,L'\0');
        SendMessageW(item.hwndItem,isCombo?CB_GETLBTEXT:LB_GETTEXT,item.itemID,reinterpret_cast<LPARAM>(label.data()));
        label.resize(size_t(length));
        theme::drawRow(item,label,id==Timeline?(item.itemID==cursor_?theme::accent:theme::line):CLR_INVALID);
    }

    static std::vector<std::string> readModels(const std::vector<game::ModManifest>& mods) {
        std::vector<std::string> models;
        for(const auto& mod:mods) {
            std::ifstream input(mod.root/"assets.json",std::ios::binary);
            if(!input) continue;
            const auto registry=nlohmann::json::parse(input);
            if(registry.contains("models")) for(const auto& [id,entry]:registry.at("models").items()) { (void)entry;models.push_back(id); }
            if(registry.contains("studio")) for(const auto& entry:registry.at("studio")) {
                const auto path=game::VirtualFileSystem::contained(mod.root,entry.at("source").get<std::string>());
                std::ifstream manifest(path,std::ios::binary);
                const auto package=nlohmann::json::parse(manifest);
                for(const auto& model:package.at("models")) models.push_back(model.at("id").get<std::string>());
            }
        }
        return models;
    }

    void showModels(std::vector<std::string> models) {
        modelIds_=std::move(models);SendMessageW(props_,CB_RESETCONTENT,0,0);
        SendMessageW(props_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Colocar modelo…"));
        for(const auto& id:modelIds_) {
            const auto label=wide(id);
            SendMessageW(props_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(props_,CB_SETCURSEL,0,0);
    }

    void useContent(const std::filesystem::path& directory) {
        auto discovery=game::discoverMods(directory);
        for(const auto& diagnostic:discovery.diagnostics) if(diagnostic.code!="MOD106") throw game::DiagnosticError(diagnostic);
        auto next=WorldDefinition::load(discovery.mods);
        auto models=readModels(discovery.mods);
        mods_=directory;catalog_=std::move(next);
        ids_.clear();colors_.clear();
        for(const auto& [id,block]:catalog_.blocks) {
            (void)block;
            colors_.emplace(id,blockColor(id));
            ids_.push_back(id);
        }
        structureIds_.clear();SendMessageW(structures_,CB_RESETCONTENT,0,0);
        SendMessageW(structures_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Sem estrutura"));
        for(const auto& [id,structure]:catalog_.structures) {
            (void)structure;structureIds_.push_back(id);
            const auto label=assetName(id);SendMessageW(structures_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(structures_,CB_SETCURSEL,0,0);
        filterPalette();showModels(std::move(models));previewCatalogDirty_=true;previewDirty_=true;
    }

    void filterPalette() {
        if(!filter_) return;
        wchar_t query[256]{};GetWindowTextW(filter_,query,int(std::size(query)));
        const auto fold=[](std::wstring value) {for(auto& c:value) c=wchar_t(towlower(c));return value;};
        const auto search=fold(query);
        const auto old=SendMessageW(palette_,LB_GETCURSEL,0,0);
        const auto previous=old>=0 && size_t(old)<visibleIds_.size()?visibleIds_[size_t(old)]:size_t(-1);
        SendMessageW(palette_,WM_SETREDRAW,FALSE,0);
        SendMessageW(palette_,LB_RESETCONTENT,0,0);
        visibleIds_.clear();
        for(size_t i=0;i<ids_.size();++i) {
            const auto label=blockNameFor(ids_[i]);
            if(fold(label).find(search)==std::wstring::npos && fold(wide(ids_[i])).find(search)==std::wstring::npos) continue;
            visibleIds_.push_back(i);
            SendMessageW(palette_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        }
        if(!visibleIds_.empty()) {
            const auto selection=std::find(visibleIds_.begin(),visibleIds_.end(),previous);
            SendMessageW(palette_,LB_SETCURSEL,selection==visibleIds_.end()?0:selection-visibleIds_.begin(),0);
        }
        SendMessageW(palette_,WM_SETREDRAW,TRUE,0);
        InvalidateRect(palette_,nullptr,TRUE);
    }

    void layout() {
        if(!ready_) return;
        RECT client{};GetClientRect(window_,&client);
        const int right=int(client.right),bottom=int(client.bottom)-footer,side=right-inspector;
        const auto place=[&](int id,int x,int y,int w,int h,bool visible=true) {
            const auto control=GetDlgItem(window_,id);
            MoveWindow(control,x,y,w,h,FALSE);ShowWindow(control,visible?SW_SHOWNA:SW_HIDE);
        };
        const bool map=page_==Page::Map, home=page_==Page::Home && !regionCommand_, settings=page_==Page::Settings;
        const bool library=map && panel_==MapPanel::Library, placement=map && panel_==MapPanel::Placement;
        layoutTabs(right);
        place(ReloadBlockbench,right-118,76,100,29,page_==Page::Blockbench);
        place(ProjectMenu,18,78,93,32,map);place(SaveMap,117,78,69,32,map);
        place(Undo,194,78,76,32,map);place(Redo,276,78,71,32,map);
        place(View2D,365,78,62,32,map);place(View3D,433,78,50,32,map);
        place(TestMap,right-154,78,136,32,map);
        place(BrushTool,18,124,74,32,map);place(EraseTool,98,124,74,32,map);
        place(PickTool,178,124,78,32,map);place(FillTool,262,124,89,32,map);
        place(LayerDown,side-gutter-114,124,31,32,map);place(LayerUp,side-gutter-31,124,31,32,map);
        place(ResetCamera,side-gutter-224,124,96,32,map);
        place(LibraryPanel,side+14,124,73,32,map);place(PlacementPanel,side+93,124,88,32,map);
        place(HistoryPanel,side+187,124,93,32,map);
        place(NewBlock,right-83,173,68,29,library);place(BlockEdit,right-151,173,62,29,library);
        place(SearchLabel,side+14,202,266,22,library);
        place(Filter,side+14,227,266,33,library);place(Palette,side+14,272,266,std::max(55,bottom-350),library);
        place(Structure,side+14,215,266,180,placement);place(Props,side+14,276,266,180,placement);
        place(Rotation,side+14,335,126,125,placement);place(PropScale,side+154,335,126,33,placement);
        place(RegisterStructure,side+14,386,266,31,placement);place(LinkRoom,side+14,424,266,31,placement);
        place(SyncStudio,side+14,462,266,31,placement);
        place(Timeline,side+14,211,266,std::max(60,bottom-265),map && panel_==MapPanel::History);
        const auto region=regionPanel();
        for(int i=0;i<4;++i) place(CropX+i,region.left+18+i*101,region.top+113,90,31,regionCommand_!=0);
        place(CancelRegion,region.right-209,region.bottom-47,88,30,regionCommand_!=0);
        place(ApplyRegion,region.right-112,region.bottom-47,94,30,regionCommand_!=0);

        const int x=std::max(28,(right-1110)/2),w=std::min(1110,right-2*x);
        const int card=(w-48)/5;
        place(HomeNew,x+24,268,124,36,home);place(HomeOpen,x+157,268,132,36,home);
        place(HomeMap,x+24,322,188,33,home);
        const std::array homeTools{HomeBlocks,HomeGear,BlockbenchTab,HomeAi,Settings};
        for(int i=0;i<int(homeTools.size());++i) place(homeTools[i],x+i*(card+12)+12,494,card-24,34,home);
        place(BlocksCreate,x+24,255,150,36,page_==Page::Blocks);
        place(BlockImport,x+195,255,176,36,page_==Page::Blocks);
        place(GearCreate,x+24,255,164,36,page_==Page::Gear);
        place(AiStart,x+24,255,164,36,page_==Page::Ai);
        place(LaunchBlockbench,x+195,255,176,36,page_==Page::Gear);
        const bool authoring=page_==Page::BlockModel;
        place(BlockName,77,77,223,32,authoring);place(BlockSolid,310,77,118,32,authoring);
        place(BlockSave,438,77,143,32,authoring);place(BlockReturn,591,77,113,32,authoring);
        place(ReloadBlockbench,right-118,76,100,29,page_==Page::Blockbench || authoring);
        place(OpenBlockbenchFile,x+340,506,235,34,settings);
        place(Start3D,x+340,202,130,34,settings);place(GridToggle,x+340,248,130,34,settings);
        place(CameraSpeed,x+340,294,130,34,settings);
        place(ChooseBlockbench,x+340,406,190,34,settings);place(ChooseBridge,x+340,456,190,34,settings);
        if(renderSurface_) {
            const auto area=previewViewport();
            ShowWindow(renderSurface_,map && view3d_ && !regionCommand_ && !smoke_?SW_SHOWNA:SW_HIDE);
            MoveWindow(renderSurface_,area.left,area.top,area.right-area.left,area.bottom-area.top,FALSE);
            if(area.right-area.left>1 && area.bottom-area.top>1) previewRenderer_->resize(area.right-area.left,area.bottom-area.top);
        }
        if(blockbenchPane_) blockbenchPane_->show({0,115,right,bottom},page_==Page::Blockbench);
        for(auto& tab:tabs_) if(tab->block)
            tab->block->pane->show({0,115,right,bottom},tab.get()==currentTab_);
        RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    void refresh(const std::wstring& detail=L"") {
        status_=detail;
        if(currentTab_ && currentTab_->block) {
            const auto& block=*currentTab_->block;
            const auto solidity=block.solid?L"Sólido":L"Sem colisão";
            SetWindowTextW(GetDlgItem(window_,BlockSolid),solidity);
            EnableWindow(GetDlgItem(window_,BlockSave),block.ready && !block.publishing);
            EnableWindow(GetDlgItem(window_,BlockReturn),!block.id.empty() && !block.publishing);
            EnableWindow(GetDlgItem(window_,BlockName),!block.publishing);
            EnableWindow(GetDlgItem(window_,BlockSolid),!block.publishing);
            EnableWindow(GetDlgItem(window_,ReloadBlockbench),!block.publishing);
        }
        if(page_==Page::Map) EnableWindow(GetDlgItem(window_,BlockEdit),!editableBlock(selectedBlock()).empty());
        updateTabLabels();
        EnableWindow(GetDlgItem(window_,Undo),cursor_>0);
        EnableWindow(GetDlgItem(window_,Redo),cursor_<history_.size());
        EnableWindow(GetDlgItem(window_,SyncStudio),SendMessageW(props_,CB_GETCURSEL,0,0)>0);
        SendMessageW(timeline_,LB_SETCURSEL,cursor_,0);
        InvalidateRect(GetDlgItem(window_,View2D),nullptr,FALSE);
        InvalidateRect(GetDlgItem(window_,View3D),nullptr,FALSE);
        for(int id=BrushTool;id<=FillTool;++id) InvalidateRect(GetDlgItem(window_,id),nullptr,FALSE);
        InvalidateRect(window_,nullptr,FALSE);
    }

    bool dirty() const { return needsSave_ || cursor_!=saved_; }
    bool canReplace() {
        if(!dirty()) return true;
        const auto title=L"Salvar as alterações em "+(loadedMap_?tabLabel(*loadedMap_):L"este mapa")+L"?";
        const int decision=MessageBoxW(window_,title.c_str(),L"ShardBound Editor",MB_YESNOCANCEL|MB_ICONQUESTION);
        return decision==IDNO || (decision==IDYES && save());
    }

    std::filesystem::path chooseFile(bool writing,const wchar_t* title,const wchar_t* filter=L"Arquivos JSON\0*.json\0Todos os arquivos\0*.*\0") const {
        std::array<wchar_t,32768> name{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;dialog.lpstrFile=name.data();
        dialog.nMaxFile=DWORD(name.size());dialog.lpstrTitle=title;dialog.lpstrDefExt=L"json";
        dialog.lpstrFilter=filter;
        dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(writing?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
        const BOOL selected=writing?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);
        return selected?std::filesystem::path(name.data()):std::filesystem::path{};
    }

    void writeProject(const std::filesystem::path& target) const {
        map_.validateDoors(catalog_);
        auto document=map_.json();
        document["editor_content"]=game::pathText(mods_);
        const auto absolute=std::filesystem::absolute(target);
        if(!history_.empty()) document["editor_timeline"]=game::writeEditorTimeline(absolute,history_,cursor_);
        const auto encoded=document.dump();
        if(encoded.size()>8*1024*1024) throw std::runtime_error("O mapa excede 8 MiB");
        game::atomicWrite(absolute,encoded,true);
    }

    bool save() {
        const auto target=path_.empty()?chooseFile(true,L"Salvar mapa"):path_;
        if(target.empty()) return false;
        if(const auto existing=findMapFile(target);existing && existing!=loadedMap_)
            throw std::runtime_error("Este arquivo já está aberto em outra aba.");
        writeProject(target);
        path_=target;saved_=cursor_;needsSave_=false;refresh(L"Mapa salvo.");
        return true;
    }

    void openProject(const std::filesystem::path& path,bool separateTab=false) {
        if(separateTab) if(auto existing=findMapFile(path)) {activate(*existing);return;}
        auto content=defaultMods_;
        if(std::filesystem::file_size(path)>8*1024*1024) throw std::runtime_error("O mapa excede 8 MiB");
        nlohmann::json document;
        {std::ifstream input(path,std::ios::binary);document=nlohmann::json::parse(input);}
        const auto sidecar=std::filesystem::path(path.wstring()+L".editor.json");
        if(document.contains("editor_content")) content=wide(document.at("editor_content").get<std::string>());
        else if(std::filesystem::is_regular_file(sidecar)) {
            if(std::filesystem::file_size(sidecar)>32768) throw std::runtime_error("Contexto do editor excede o limite");
            std::ifstream input(sidecar,std::ios::binary);
            const auto context=nlohmann::json::parse(input);
            if(context.at("version")!=1) throw std::runtime_error("Versão do contexto do editor indisponível");
            content=wide(context.at("content").get<std::string>());
        }
        if(!content.is_absolute() && !content.empty() && !content.has_root_path()) {
            const auto root=std::filesystem::canonical(std::filesystem::absolute(path).parent_path());
            const auto resolved=std::filesystem::weakly_canonical(root/content);
            const auto relative=resolved.lexically_relative(root);
            if(!relative.empty() && *relative.begin()!=L"..") content=resolved;
        }
        if(!content.is_absolute() || !std::filesystem::is_directory(content))
            throw std::runtime_error("Pasta de mods do mapa indisponível");
        const auto definition=content==mods_?catalog_:WorldDefinition::load(game::discoverMods(content).mods);
        auto loaded=FiniteMap::parse(document,definition);
        game::EditorTimeline timeline;
        std::wstring timelineError;
        if(document.contains("editor_timeline")) {
            try {
                timeline=game::readEditorTimeline(path,document.at("editor_timeline").get<std::string>(),loaded,content,definition);
            } catch(const std::exception& error) {
                timelineError=L"O mapa foi aberto. A linha do tempo ficou indisponível.\n\n"+wide(error.what());
            }
        }
        if(separateTab) addMapTab(std::move(loaded),path,content,false,std::move(timeline));
        else {
            if(content!=mods_) useContent(content);
            replace(std::move(loaded),path,false);
            history_=std::move(timeline.edits);
            cursor_=saved_=timeline.cursor;
            rebuildTimeline();
        }
        refresh(timelineError.empty()?L"Mapa aberto.":L"Mapa aberto. Linha do tempo indisponível.");
        if(!timelineError.empty() && !smoke_) MessageBoxW(window_,timelineError.c_str(),L"ShardBound Editor",MB_ICONWARNING);
    }

    int number(size_t index) const {
        wchar_t text[32]{};GetWindowTextW(crop_[index],text,32);
        size_t parsed=0;const int value=std::stoi(text,&parsed);
        if(text[parsed]) throw std::runtime_error("Use coordenadas e dimensões inteiras");
        return value;
    }

    void replace(FiniteMap map,std::filesystem::path path,bool modified) {
        map_=std::move(map);path_=std::move(path);needsSave_=modified;
        previewDirty_=true;centerPreview();
        roomToLink_.reset();entranceToLink_.reset();
        selectingStructure_=false;structureCorner_.reset();
        fillCorner_.reset();plantScale_=1;plantX_=plantY_=0;
        history_.clear();cursor_=saved_=0;
        rebuildTimeline();
    }

    void rebuildTimeline() const {
        SendMessageW(timeline_,LB_RESETCONTENT,0,0);
        SendMessageW(timeline_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Início do histórico"));
        for(const auto& edit:history_) SendMessageW(timeline_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(edit.label.c_str()));
    }

    void projectMenu() const {
        const auto menu=CreatePopupMenu();
        if(!menu) return;
        AppendMenuW(menu,MF_STRING,NewMap,L"Novo mapa…");
        AppendMenuW(menu,MF_STRING,OpenMap,L"Abrir mapa…\tCtrl+O");
        AppendMenuW(menu,MF_STRING,ImportSave,L"Importar região de um save…");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,ExportMap,L"Exportar mapa e mods…");
        AppendMenuW(menu,MF_STRING,ExportContent,L"Exportar mods da revisão…");
        RECT anchor{};GetWindowRect(GetDlgItem(window_,ProjectMenu),&anchor);
        TrackPopupMenu(menu,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_LEFTBUTTON,anchor.left,anchor.bottom+4,0,window_,nullptr);
        DestroyMenu(menu);
    }

    void command(int id) {
        if((id>=tabSelectBase && id<tabSelectBase+tabLimit) || (id>=tabCloseBase && id<tabCloseBase+tabLimit)) {
            const bool close=id>=tabCloseBase;
            const int slot=id-(close?tabCloseBase:tabSelectBase);
            for(const auto& tab:tabs_) if(tab->slot==slot) {
                if(close) closeTab(*tab);else activate(*tab,true);
                break;
            }
            return;
        }
        if(id==NewTab) {activate(addTab(Page::Home,L"Nova aba"),true);return;}
        if(id==CloseTab) {closeTab(*currentTab_);return;}
        if(id==CycleTabs || id==ReverseTabs) {
            const auto index=tabIndex(*currentTab_);
            const auto step=id==CycleTabs?1:tabs_.size()-1;
            activate(*tabs_[(index+step)%tabs_.size()],true);
            return;
        }
        if(id==PreviousTabs || id==NextTabs) {
            if(id==PreviousTabs && firstTab_>0) --firstTab_;
            if(id==NextTabs) ++firstTab_;
            layout();return;
        }
        if(id==BlockbenchTab || id==LaunchBlockbench) {go(Page::Blockbench);return;}
        if(id==ReloadBlockbench) {
            auto* draft=currentTab_->block.get();
            auto* pane=draft?draft->pane.get():blockbenchPane_.get();
            if(pane && (!(draft?draft->ready && draft->changed:true) || MessageBoxW(window_,
                L"Recarregar descarta alterações ainda não salvas.",L"Recarregar Blockbench",MB_OKCANCEL|MB_ICONQUESTION)==IDOK)) {
                if(draft) {draft->ready=false;draft->status=L"Carregando o modelo de bloco…";refresh();}
                pane->retry();
            }
            return;
        }
        if(id==BlockSave || (id==SaveMap && page_==Page::BlockModel)) {requestBlockSave(*currentTab_);return;}
        if(id==BlockSolid) {
            auto& draft=*currentTab_->block;
            draft.solid=!draft.solid;markBlockChanged(*currentTab_);refresh(draft.status);return;
        }
        if(id==BlockReturn) {returnToBlock(*currentTab_->block);return;}
        if(id==BlockEdit) {editSelectedBlock();return;}
        if(id==NewBlock) {openBlock();return;}
        if(id==Settings) {go(Page::Settings);return;}
        if(id==HomeNew || id==HomeOpen) {command(id==HomeNew?NewMap:OpenMap);return;}
        if(id==HomeMap) {go(Page::Map);return;}
        if(id>=HomeBlocks && id<=HomeAi) {go(Page(UINT(Page::Blocks)+id-HomeBlocks));return;}
        if(id==BlocksCreate || id==GearCreate || id==AiStart) {command(id==BlocksCreate?NewBlock:id==GearCreate?Equipment:Ai);return;}
        if(id==OpenBlockbenchFile) {command(OpenBlockbench);return;}
        if(id>=LibraryPanel && id<=HistoryPanel) {panel_=MapPanel(id-LibraryPanel);layout();return;}
        if(id>=Start3D && id<=ChooseBridge) {changeSetting(id);return;}
        if(id>=BrushTool && id<=FillTool) {selectTerrainTool(TerrainTool(id-BrushTool));return;}
        if(id==ResetCamera) {
            pitch_=.88f;orbit_=.72f;plantScale_=1;plantX_=plantY_=0;
            centerPreview();SetFocus(window_);refresh();return;
        }
        if(id==ProjectMenu) {projectMenu();return;}
        if(id==NewMap || id==ImportSave) {
            cancelCameraGesture();
            regionCommand_=id;
            layout();SetFocus(crop_[0]);return;
        }
        if(id==CancelRegion) {regionCommand_=0;layout();SetFocus(window_);return;}
        if(id==ApplyRegion && regionCommand_) {
            const FiniteMap::Bounds bounds{number(0),number(1),number(2),number(3)};
            if(regionCommand_==NewMap) addMapTab(FiniteMap(bounds),{},mods_,true);
            else {
                const auto checkpoint=chooseFile(false,L"Abrir checkpoint.json do ShardBound");
                if(checkpoint.empty()) return;
                auto imported=game::importMapFromSave(checkpoint,bounds);
                addMapTab(FiniteMap::parse(imported.json(),catalog_),{},mods_,true);
            }
            regionCommand_=0;layout();SetFocus(window_);refresh();return;
        }
        if(id==OpenMap) {
            const auto path=chooseFile(false,L"Abrir mapa finito");
            if(!path.empty()) openProject(path,true);
            return;
        }
        if((id==SaveMap || id==Undo || id==Redo) && page_!=Page::Map) return;
        if(id==SaveMap) { save();return; }
        if(id==TestMap) { testMap();return; }
        if(id==ExportContent) {
            const auto selection=chooseFile(true,L"Escolher destino (o nome cria uma pasta de mods)");
            if(!selection.empty()) {
                const auto output=selection.parent_path()/selection.stem();
                if(std::filesystem::exists(output)) throw std::runtime_error("O destino já existe");
                const auto root=std::filesystem::canonical(mods_);
                const auto resolved=std::filesystem::weakly_canonical(output);
                const auto relative=resolved.lexically_relative(root);
                if(!relative.empty() && *relative.begin()!=L"..") throw std::runtime_error("Escolha um destino fora da pasta de origem");
                std::filesystem::copy(mods_,output,std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks);
                refresh(L"Mods da revisão exportados.");
            }
            return;
        }
        if(id==ExportMap) {
            const auto output=chooseFile(true,L"Escolher o nome da pasta do pacote (ex.: castelo.json)");
            if(!output.empty()) {
                game::exportMapPackage(map_,mods_,output.parent_path()/output.stem(),programDirectory_/"modcheck.exe");
                refresh(L"Pacote de mapa e mods exportado.");
            }
            return;
        }
        if(id==View3D || id==View2D) {
            if(!regionCommand_ && (id==View3D)!=view3d_) togglePreview();
            return;
        }
        if(id==SyncStudio) { syncStudio();return; }
        if(id==RegisterStructure) {
            selectingStructure_=!selectingStructure_;structureCorner_.reset();
            refresh(selectingStructure_?L"Clique no primeiro canto da estrutura. A camada atual define a base.":L"Seleção cancelada.");return;
        }
        if(id==Ai) {
            auto draft=game::authorMapWithAi(window_,map_,catalog_,mods_,programDirectory_,bridge_);
            if(!draft.revisions.empty()) {
                attachDraftMap();
                auto before=std::make_shared<const FiniteMap>(map_);
                auto content=mods_;
                history_.reserve(cursor_+draft.revisions.size());
                const auto finalContent=draft.revisions.back().content;
                if(mods_!=finalContent) useContent(finalContent);
                for(auto& revision:draft.revisions) {
                    Edit edit;edit.label=L"IA · "+wide(revision.label);
                    auto after=std::make_shared<const FiniteMap>(std::move(revision.map));
                    if(content!=revision.content) {edit.content=std::pair{content,revision.content};content=std::move(revision.content);}
                    edit.revision=std::pair{before,after};before=std::move(after);record(std::move(edit));
                }
                map_=*before;centerPreview();refresh(L"Rascunho aplicado ao mapa.");
            }
            return;
        }
        if(id==Equipment) {
            if(const auto equipment=game::editEquipment(window_,programDirectory_,bridge_)) {
                const auto next=copyContent("equipment");
                game::publishEquipment(next,*equipment);
                recordContent(next,L"Registrar equipamento");
                refresh(L"Equipamento registrado como mod.");
            }
            return;
        }
        if(id==BlockImport) {
            if(const auto block=game::editNewBlock(window_)) {
                if(catalog_.blocks.size()>=16384) throw std::runtime_error("O catálogo atingiu o limite de blocos");
                const auto next=copyContent("block");
                game::publishBlock(next,*block);
                const auto name=block->mod+":block";
                recordContent(next,L"Registrar bloco · "+wide(name));
                if(page_==Page::Map) {
                    const auto label=wide(name);SetWindowTextW(filter_,label.c_str());filterPalette();
                }
                SendMessageW(structures_,CB_SETCURSEL,0,0);
                refresh(L"Bloco registrado. Clique no mapa para colocá-lo.");
            }
            return;
        }
        if(id==LinkRoom) {
            roomToLink_.reset();entranceToLink_.reset();
            const auto path=chooseFile(false,L"Escolher um mapa de interior com ponto inicial e saída");
            if(!path.empty()) {
                auto room=FiniteMap::read(path,catalog_);
                if(!room.exit() || !room.interiors().empty()) throw std::runtime_error("O interior precisa de uma saída e de um ponto inicial distintos");
                roomToLink_=std::make_shared<FiniteMap>(std::move(room));
                refresh(L"Clique na posição da porta de entrada no mapa.");
            }
            return;
        }
        if(id==OpenBlockbench) {
            const auto project=chooseFile(false,L"Abrir projeto no Blockbench",L"Projetos Blockbench\0*.bbmodel;*.sbstudio\0");
            if(!project.empty()) {
                if(resolveBlockbench()) open(blockbench_,L"\""+project.wstring()+L"\"");
            }
            return;
        }
        if(id==Props && SendMessageW(props_,CB_GETCURSEL,0,0)>0) {SendMessageW(structures_,CB_SETCURSEL,0,0);fillCorner_.reset();}
        if(id==Structure && SendMessageW(structures_,CB_GETCURSEL,0,0)>0) {SendMessageW(props_,CB_SETCURSEL,0,0);fillCorner_.reset();}
        if(id==LayerDown) layer_=std::max(0,layer_-1);
        if(id==LayerUp) layer_=std::min(FiniteMap::height-1,layer_+1);
        if(id==Undo && cursor_>0) {
            apply(history_[cursor_-1],false);
            --cursor_;
        }
        if(id==Redo && cursor_<history_.size()) {
            apply(history_[cursor_],true);
            ++cursor_;
        }
        refresh();
    }

    void open(const std::filesystem::path& program,const std::wstring& arguments) const {
        const auto workingDirectory=program.parent_path();
        const auto result=ShellExecuteW(window_,L"open",program.c_str(),arguments.c_str(),workingDirectory.c_str(),SW_SHOWNORMAL);
        if(reinterpret_cast<INT_PTR>(result)<=32) throw std::runtime_error("Não foi possível iniciar o aplicativo");
    }

    void activate(Tab& tab,bool focusTab=false) {
        cancelCameraGesture();
        if(tab.page==Page::Blockbench && !blockbenchPane_)
            blockbenchPane_=std::make_unique<game::editor::BlockbenchPane>(
                window_,game::defaultUserDirectory(programDirectory_)/"blockbench-web");
        if(tab.page==Page::Map && &tab!=loadedMap_) loadMapTab(tab);
        if(currentTab_!=&tab) regionCommand_=0;
        currentTab_=&tab;page_=tab.page;
        if(tab.block) {
            settingBlockFields_=true;
            SetWindowTextW(GetDlgItem(window_,BlockName),tab.block->name.c_str());
            settingBlockFields_=false;
        }
        if(page_==Page::Map && view3d_ && !previewRenderer_) {
            view3d_=false;
            togglePreview();
        }
        if(page_==Page::Home) SetWindowTextW(GetDlgItem(window_,HomeMap),
            loadedMap_?L"Voltar ao mapa atual":L"Editar mapa de exemplo");
        revealTab(tab);layout();SetFocus(focusTab?tab.select:window_);refresh(tab.block?tab.block->status:L"");
    }

    void useLauncher(Tab& tab,Tab* launcher) {
        if(!launcher || launcher->page!=Page::Home || !launcher->close) return;
        std::swap(tabs_[tabIndex(tab)],tabs_[tabIndex(*launcher)]);
        eraseTab(*launcher);
        revealTab(tab);layout();
    }

    void go(Page next) {
        if(next==Page::Map) {
            if(loadedMap_) activate(*loadedMap_);
            else {
                auto example=catalog_.maps.empty()?FiniteMap({0,0,64,64}):*catalog_.maps.begin()->second;
                addMapTab(std::move(example),{},mods_,false,{},L"Mapa de exemplo");
            }
            return;
        }
        if(auto existing=findTab(next)) {activate(*existing);return;}
        const auto launcher=currentTab_;
        const std::array names{L"Início",L"Mapa",L"Blocos",L"Equipamentos",L"Assistente IA",L"Configurações",L"Blockbench"};
        auto& tab=addTab(next,names[size_t(next)]);
        try {activate(tab);}
        catch(...) {eraseTab(tab);layout();throw;}
        useLauncher(tab,launcher);
    }

    void attachDraftMap() {
        if(loadedMap_) return;
        loadedMap_=&addTab(Page::Map,L"Mapa de exemplo");
        centerPreview();view3d_=start3d_;
        layout();
    }

    Tab* findMapFile(const std::filesystem::path& file) const {
        for(const auto& tab:tabs_) if(tab->page==Page::Map) {
            const auto& path=tab.get()==loadedMap_?path_:tab->map->path;
            std::error_code error;
            if(!path.empty() && std::filesystem::equivalent(path,file,error)) return tab.get();
        }
        return nullptr;
    }

    static std::string selectedId(HWND control,UINT message,const std::vector<std::string>& ids,bool hasEmpty) {
        const auto selection=SendMessageW(control,message,0,0)-int(hasEmpty);
        return selection>=0 && size_t(selection)<ids.size()?ids[size_t(selection)]:std::string{};
    }

    void loadMapTab(Tab& tab) {
        std::optional<MapSession> previous;
        if(loadedMap_) {
            const auto selection=SendMessageW(palette_,LB_GETCURSEL,0,0);
            auto block=selection>=0 && size_t(selection)<visibleIds_.size()?ids_[visibleIds_[size_t(selection)]]:std::string{};
            auto mods=mods_;
            auto filter=controlText(filter_),scale=controlText(propScale_);
            auto structure=selectedId(structures_,CB_GETCURSEL,structureIds_,true);
            auto model=selectedId(props_,CB_GETCURSEL,modelIds_,true);
            previous.emplace(MapSession{std::move(map_),std::move(path_),std::move(mods),std::move(history_),cursor_,saved_,
                needsSave_,view3d_,layer_,panel_,terrainTool_,std::move(filter),std::move(scale),std::move(block),
                std::move(structure),std::move(model),
                int(SendMessageW(rotation_,CB_GETCURSEL,0,0)),centerX_,centerZ_,orbit_,distance_,pitch_,plantScale_,plantX_,plantY_});
        }
        auto& next=*tab.map;
        try {
            if(next.mods!=mods_) useContent(next.mods);
        } catch(...) {
            if(previous) {
                map_=std::move(previous->map);path_=std::move(previous->path);history_=std::move(previous->history);
            }
            throw;
        }
        if(loadedMap_) loadedMap_->map=std::move(previous);
        map_=std::move(next.map);path_=std::move(next.path);history_=std::move(next.history);
        cursor_=next.cursor;saved_=next.saved;needsSave_=next.dirty;
        view3d_=next.view3d;layer_=next.layer;panel_=next.panel;terrainTool_=next.tool;
        centerX_=next.x;centerZ_=next.z;orbit_=next.orbit;distance_=next.distance;pitch_=next.pitch;
        plantScale_=next.zoom;plantX_=next.panX;plantY_=next.panY;
        previewDirty_=true;
        roomToLink_.reset();entranceToLink_.reset();selectingStructure_=false;structureCorner_.reset();fillCorner_.reset();
        SetWindowTextW(filter_,next.filter.c_str());filterPalette();
        SetWindowTextW(propScale_,next.propScale.c_str());
        if(!visibleIds_.empty()) SendMessageW(palette_,LB_SETCURSEL,0,0);
        for(size_t i=0;i<visibleIds_.size();++i) if(ids_[visibleIds_[i]]==next.block) {
            SendMessageW(palette_,LB_SETCURSEL,i,0);break;
        }
        const auto selectCombo=[](HWND control,const std::vector<std::string>& ids,const std::string& id) {
            const auto found=std::find(ids.begin(),ids.end(),id);
            SendMessageW(control,CB_SETCURSEL,found==ids.end()?0:1+(found-ids.begin()),0);
        };
        selectCombo(structures_,structureIds_,next.structure);
        selectCombo(props_,modelIds_,next.model);
        SendMessageW(rotation_,CB_SETCURSEL,std::max(0,next.rotation),0);
        rebuildTimeline();
        loadedMap_=&tab;tab.map.reset();
    }

    Tab& addMapTab(FiniteMap map,std::filesystem::path path,std::filesystem::path mods,bool modified,
        game::EditorTimeline timeline={},std::wstring name={}) {
        const auto launcher=currentTab_;
        if(name.empty() && path.empty()) name=L"Mapa "+std::to_wstring(nextMap_++);
        const auto bounds=map.bounds();
        auto& tab=addTab(Page::Map,std::move(name));
        tab.map.emplace(MapSession{std::move(map),std::move(path),std::move(mods),std::move(timeline.edits),
            timeline.cursor,timeline.cursor,modified,start3d_,3,MapPanel::Library,TerrainTool::Brush,L"",L"1",
            {},{},{},0,float(bounds.minX)+bounds.width*.5f,float(bounds.minZ)+bounds.depth*.5f,.72f,
            std::clamp(float(std::hypot(bounds.width,bounds.depth))*1.23f,12.f,900.f),.88f,1.f,0.f,0.f});
        try {activate(tab);}
        catch(...) {
            if(loadedMap_!=&tab) {eraseTab(tab);layout();}
            throw;
        }
        useLauncher(tab,launcher);
        return tab;
    }

    void eraseTab(Tab& tab) {
        const auto index=tabIndex(tab);
        DestroyWindow(tab.select);DestroyWindow(tab.close);
        tabs_.erase(tabs_.begin()+index);
        if(index<firstTab_) --firstTab_;
    }

    void closeTab(Tab& tab) {
        if(!tab.close) return;
        if(tab.block && (tab.block->publishing || (tab.block->changed && !confirmBlockDraft(*tab.block)))) return;
        if(tab.page==Page::Map && tabDirty(tab)) {
            activate(tab);
            if(!canReplace()) return;
        }
        if(tab.page==Page::Map) {
            for(const auto& item:tabs_) if(item->block && item->block->map==&tab &&
                (item->block->publishing || (item->block->changed && !confirmBlockDraft(*item->block)))) return;
        }
        if(tab.page==Page::Blockbench) {
            if(!confirmBlockbenchClose()) return;
            blockbenchPane_.reset();
        }
        if(&tab==loadedMap_) {
            const auto next=std::find_if(tabs_.rbegin(),tabs_.rend(),[&](const auto& item) {
                return item.get()!=&tab && item->page==Page::Map;
            });
            if(next!=tabs_.rend()) loadMapTab(**next);
            else {
                if(mods_!=defaultMods_) useContent(defaultMods_);
                replace(catalog_.maps.empty()?FiniteMap({0,0,64,64}):*catalog_.maps.begin()->second,{},false);
                loadedMap_=nullptr;view3d_=false;
            }
        }
        if(tab.page==Page::Map) {
            for(size_t i=tabs_.size();i-->0;) {
                auto& item=*tabs_[i];
                if(item.block && item.block->map==&tab) {
                    if(currentTab_==&item) currentTab_=&tab;
                    eraseTab(item);
                }
            }
        }
        const auto index=tabIndex(tab);
        const bool wasActive=&tab==currentTab_;
        if(wasActive) currentTab_=nullptr;
        eraseTab(tab);
        if(wasActive) activate(*tabs_[std::min(index,tabs_.size()-1)],true);
        else {revealTab(*currentTab_);layout();refresh();}
    }

    bool confirmAllTabs() {
        for(const auto& tab:tabs_) if(tab->block) {
            if(tab->block->publishing || (tab->block->changed && !confirmBlockDraft(*tab->block))) return false;
        }
        for(const auto& tab:tabs_) if(tab->page==Page::Map && tabDirty(*tab)) {
            activate(*tab);
            if(!canReplace()) return false;
        }
        return !blockbenchPane_ || confirmBlockbenchClose();
    }

    bool confirmBlockbenchClose() const {
        return MessageBoxW(window_,L"Fechar o Blockbench? Salve os modelos abertos dentro dele antes de fechar.",
            L"Fechar Blockbench",MB_OKCANCEL|MB_ICONQUESTION)==IDOK;
    }

    bool confirmBlockDraft(const BlockDraft& draft) const {
        const auto message=L"Fechar “"+draft.name+L"”? As alterações depois do último salvamento serão descartadas.";
        return MessageBoxW(window_,message.c_str(),L"Fechar bloco",MB_OKCANCEL|MB_ICONQUESTION)==IDOK;
    }

    std::wstring blockNameFor(const std::string& id) const {
        const auto found=catalog_.blocks.find(id);
        if(found!=catalog_.blocks.end() && !found->second.name.empty()) return wide(found->second.name);
        return assetName(id);
    }

    std::string selectedBlock() const {
        const auto index=SendMessageW(palette_,LB_GETCURSEL,0,0);
        return index>=0 && size_t(index)<visibleIds_.size()?ids_[visibleIds_[size_t(index)]]:std::string{};
    }

    std::filesystem::path editableBlock(const std::string& id) const {
        constexpr std::string_view suffix=":block";
        if(!id.ends_with(suffix)) return {};
        const auto path=mods_/id.substr(0,id.size()-suffix.size())/"block.bbmodel";
        std::error_code error;
        return std::filesystem::is_regular_file(path,error)?path:std::filesystem::path{};
    }

    static std::wstring blockTabName(const BlockDraft& draft) {
        return draft.name.empty()?L"Novo bloco":draft.name;
    }

    static std::string readBlockFile(const std::filesystem::path& path) {
        if(std::filesystem::file_size(path)>16*1024*1024) throw std::runtime_error("O projeto ultrapassa 16 MiB.");
        std::ifstream input(path,std::ios::binary);
        if(!input) throw std::runtime_error("Não foi possível ler o projeto do bloco.");
        return {std::istreambuf_iterator<char>(input),{}};
    }

    void editSelectedBlock() {
        const auto id=selectedBlock();
        if(const auto path=editableBlock(id);!path.empty()) {
            for(const auto& tab:tabs_) if(tab->block && tab->block->map==loadedMap_ && tab->block->id==id) {
                activate(*tab);return;
            }
            const auto& block=catalog_.blocks.at(id);
            openBlock(id,readBlockFile(path),blockNameFor(id),block.solid);
        }
    }

    void openBlock(std::string id={},std::string source={},std::wstring name={},bool solid=true) {
        const auto script=wide(readBlockFile(programDirectory_/"editor-blockbench.js"));
        attachDraftMap();
        if(name.empty()) name=L"Bloco "+std::to_wstring(nextBlock_++);
        auto draft=std::make_unique<BlockDraft>();
        draft->map=loadedMap_;draft->name=std::move(name);draft->solid=solid;
        draft->id=std::move(id);draft->source=std::move(source);
        draft->changed=draft->id.empty();
        auto& tab=addTab(Page::BlockModel,blockTabName(*draft));
        try {
            draft->pane=std::make_unique<game::editor::BlockbenchPane>(window_,
                blockbenchTest_?std::filesystem::temp_directory_path()/L"ShardBoundEditor"/
                    (L"blockbench-test-"+std::to_wstring(GetCurrentProcessId())):
                    game::defaultUserDirectory(programDirectory_)/"blockbench-web",script,
                [this,&tab](const nlohmann::json& event) {onBlockEvent(tab,event);});
            tab.block=std::move(draft);
            activate(tab);
        } catch(...) {
            eraseTab(tab);layout();throw;
        }
    }

    void blockStatus(Tab& tab,std::wstring status) {
        tab.block->status=std::move(status);
        if(blockbenchTest_) std::fprintf(stderr,"%s\n",utf8(tab.block->status).c_str());
        if(currentTab_==&tab) refresh(tab.block->status);
    }

    void markBlockChanged(Tab& tab) {
        tab.block->changed=true;
        ++tab.block->changes;
        updateTabLabels();
    }

    void requestBlockSave(Tab& tab) {
        auto& draft=*tab.block;
        if(!draft.ready || draft.publishing) return;
        auto name=draft.name;
        name.erase(0,name.find_first_not_of(L" \t"));
        if(name.empty()) {blockStatus(tab,L"Dê um nome ao bloco antes de salvar.");return;}
        name.erase(name.find_last_not_of(L" \t")+1);
        draft.name=std::move(name);
        draft.ticket=++nextPublication_;
        draft.savingChanges=draft.changes;
        draft.publishing=true;
        blockStatus(tab,L"Salvando o modelo e registrando o bloco…");
        try {
            const auto arguments=nlohmann::json::array({draft.ticket,utf8(draft.name)}).dump(-1,' ',true);
            draft.pane->execute(L"window.ShardBoundBlock.save(..."+wide(arguments)+L");");
        } catch(...) {
            draft.publishing=false;blockStatus(tab,L"Não foi possível iniciar o salvamento no Blockbench.");
        }
    }

    void onBlockEvent(Tab& tab,const nlohmann::json& event) {
        auto& draft=*tab.block;
        try {
            const auto kind=event.at("kind").get<std::string>();
            if(kind=="loaded") {
                draft.ready=draft.publishing=false;
                blockStatus(tab,L"Preparando o modelo de bloco…");
                nlohmann::json start=nlohmann::json::array({utf8(draft.name),nullptr});
                if(!draft.source.empty()) start[1]=nlohmann::json::parse(draft.source);
                draft.pane->execute(L"window.ShardBoundBlock.open(..."+wide(start.dump(-1,' ',true))+L");");
            } else if(kind=="ready") {
                draft.ready=true;
                blockStatus(tab,L"Modele e pinte no Blockbench. Use Salvar na engine ou Ctrl+S.");
                if(blockbenchTest_) runBlockbenchTest(tab);
            } else if(kind=="setup_error") {
                blockStatus(tab,L"Falha ao preparar o bloco. "+wide(event.at("message").get<std::string>()));
            } else if(kind=="save" && currentTab_==&tab) {
                requestBlockSave(tab);
            } else if(kind=="changed") {
                markBlockChanged(tab);
            } else if(draft.publishing && event.at("ticket")==draft.ticket) {
                draft.publishing=false;
                if(kind=="publish") {
                    publishBlockDraft(draft,event);
                    draft.changed=draft.changes!=draft.savingChanges;
                    tab.name=blockTabName(draft);
                    blockStatus(tab,L"Bloco salvo. Ele já está na paleta do mapa.");
                    if(blockbenchTest_) PostMessageW(window_,WM_APP+4,0,0);
                } else if(kind=="export_error") {
                    blockStatus(tab,L"Falha ao exportar o bloco. "+wide(event.at("message").get<std::string>()));
                }
            }
        } catch(const std::exception& error) {
            draft.publishing=false;
            blockStatus(tab,L"Não foi possível salvar ou abrir o bloco. "+wide(error.what()));
        }
    }

    static std::string nextBlockMod() {
        GUID guid{};
        if(FAILED(CoCreateGuid(&guid))) throw std::runtime_error("Não foi possível gerar um ID para o bloco.");
        wchar_t key[40]{};
        StringFromGUID2(guid,key,int(std::size(key)));
        std::string mod="bloco";
        for(const auto c:std::wstring_view(key)) if(c!=L'{' && c!=L'}') mod+=char(towlower(c));
        return mod;
    }

    static std::string decodeBlockGlb(const std::string& encoded) {
        constexpr size_t maxGlb=32*1024*1024;
        if(encoded.empty() || encoded.size()>4*((maxGlb+2)/3)) throw std::runtime_error("Modelo GLB inválido.");
        DWORD length=0;
        constexpr DWORD flags=CRYPT_STRING_BASE64|CRYPT_STRING_STRICT;
        if(!CryptStringToBinaryA(encoded.data(),DWORD(encoded.size()),flags,nullptr,&length,nullptr,nullptr) ||
            length<12 || length>maxGlb) throw std::runtime_error("Modelo GLB inválido.");
        std::string glb(length,'\0');
        if(!CryptStringToBinaryA(encoded.data(),DWORD(encoded.size()),flags,
            reinterpret_cast<BYTE*>(glb.data()),&length,nullptr,nullptr) || !glb.starts_with("glTF"))
            throw std::runtime_error("Modelo GLB inválido.");
        return glb;
    }

    void publishBlockDraft(BlockDraft& draft,const nlohmann::json& event) {
        auto source=event.at("source").get<std::string>();
        if(source.size()>16*1024*1024) throw std::runtime_error("O projeto ultrapassa 16 MiB.");
        const auto project=nlohmann::json::parse(source);
        if(project.at("meta").at("model_format")!="free" || !project.at("elements").is_array() ||
            project.at("elements").empty()) throw std::runtime_error("Projeto de bloco inválido.");
        auto glb=decodeBlockGlb(event.at("glb").get_ref<const std::string&>());
        game::AuthoredBlock block;
        block.name=utf8(draft.name);block.solid=draft.solid;
        const auto& pixels=event.at("pixels");
        if(!pixels.is_array() || pixels.size()!=block.pixels.size()) throw std::runtime_error("Prévia do bloco inválida.");
        for(size_t i=0;i<block.pixels.size();++i) {
            const auto& color=pixels[i];
            if(!color.is_number_integer() || color.get<std::uint64_t>()>0xffffff) throw std::runtime_error("Cor inválida.");
            block.pixels[i]=color.get<std::uint32_t>();
        }
        if(!draft.map) throw std::runtime_error("O mapa de destino foi fechado. Reabra o mapa para vincular este bloco.");
        auto* previous=loadedMap_;
        if(draft.map!=loadedMap_) loadMapTab(*draft.map);
        try {
            std::string replacing;
            if(draft.id.empty()) {
                if(catalog_.blocks.size()>=16384) throw std::runtime_error("O catálogo atingiu o limite de blocos.");
                block.mod=nextBlockMod();
            } else {
                block.mod=game::AssetId(draft.id).nameSpace();
                const auto existing=editableBlock(draft.id);
                if(existing.empty() && std::filesystem::exists(mods_/block.mod))
                    throw std::runtime_error("O ID do bloco já está em uso nesta revisão do mapa.");
                if(!existing.empty()) replacing=block.mod;
            }
            auto id=block.mod+":block";
            block.project=source;
            const auto revision=copyContent("blockbench",replacing);
            block.model=revision.parent_path()/"model.glb";
            game::atomicWrite(block.model,glb);
            try {game::publishBlock(revision,block);}
            catch(...) {
                std::error_code ignored;
                std::filesystem::remove(block.model,ignored);
                throw;
            }
            std::error_code ignored;
            std::filesystem::remove(block.model,ignored);
            recordContent(revision,L"Salvar bloco · "+draft.name);
            draft.id=std::move(id);draft.source=std::move(source);
            selectPublishedBlock(draft.id);
        } catch(...) {
            if(previous && previous!=loadedMap_) loadMapTab(*previous);
            throw;
        }
        if(previous && previous!=loadedMap_) loadMapTab(*previous);
    }

    void selectPublishedBlock(const std::string& id) {
        panel_=MapPanel::Library;
        SetWindowTextW(filter_,L"");filterPalette();
        for(size_t i=0;i<visibleIds_.size();++i) if(ids_[visibleIds_[i]]==id) {
            SendMessageW(palette_,LB_SETCURSEL,i,0);break;
        }
        terrainTool_=TerrainTool::Brush;
    }

    void returnToBlock(const BlockDraft& draft) {
        if(!draft.map || draft.id.empty()) return;
        activate(*draft.map);
        if(!catalog_.blocks.contains(draft.id)) {
            refresh(L"Esse bloco não está na revisão atual. Use Refazer ou salve novamente no Blockbench.");
            return;
        }
        selectPublishedBlock(draft.id);
        layout();refresh(L"Bloco selecionado. Clique no terreno para colocá-lo.");
    }

    void runBlockbenchTest(Tab& tab) {
        if(testBlockStage_==0) {
            SetWindowTextW(GetDlgItem(window_,BlockName),L"Farol de Cobre");
            tab.block->pane->execute(LR"JS(
                const texture = Texture.all[0];
                texture.edit(canvas => {
                    const brush = canvas.getContext("2d");
                    brush.fillStyle = "#e39155";
                    brush.fillRect(0, 0, 16, 16);
                });
                new Cube({name: "Topo", from: [-4, 16, -4], to: [4, 20, 4]}).init();
                Cube.all[1].applyTexture(texture, true);
                BarItems.save_project.click();
            )JS");
        } else {
            tab.block->pane->execute(LR"JS(
                if (Cube.all.length === 2 && Texture.all[0].ctx.getImageData(2, 2, 1, 1).data[0] === 227) {
                    new Cube({name: "Base", from: [-6, 0, -6], to: [6, 4, 6]}).init();
                    Cube.all[2].applyTexture(Texture.all[0], true);
                    BarItems.save_project.click();
                }
            )JS");
        }
    }

    void verifyBlockbenchResult() {
        auto& tab=*currentTab_;
        const auto id=tab.block->id;
        returnToBlock(*tab.block);
        const auto& definition=catalog_.blocks.at(id);
        if(selectedBlock()!=id || definition.name!="Farol de Cobre" || definition.model!=id)
            throw std::runtime_error("O modelo salvo não apareceu na paleta do mapa.");
        const auto source=nlohmann::json::parse(readBlockFile(editableBlock(id)));
        if(source.at("elements").size()!=2+testBlockStage_) throw std::runtime_error("O projeto editável perdeu geometria.");
        if(testBlockStage_==0) {
            if(source.at("textures").empty() || !source.at("textures").at(0).at("source").get<std::string>().starts_with("data:image/png"))
                throw std::runtime_error("A textura pintada não foi incorporada.");
            const auto area=viewport();
            paintCell((area.left+area.right)/2,(area.top+area.bottom)/2);
            const auto placed=map_.json();
            if(placed.dump().find(id)==std::string::npos) throw std::runtime_error("O bloco não pôde ser colocado no mapa.");
            togglePreview();drawPreview();togglePreview();
            const auto revision=mods_.parent_path();
            testMapPath_=revision/"farol-de-cobre.json";
            writeProject(testMapPath_);
            openProject(testMapPath_);
            selectPublishedBlock(id);
            if(selectedBlock()!=id || map_.json()!=placed) throw std::runtime_error("O mapa reaberto perdeu o bloco.");
            eraseTab(tab);
            ++testBlockStage_;
            editSelectedBlock();
        } else {
            if(id!=selectedBlock() || history_.size()<3 || catalog_.blocks.at(id).model!=id)
                throw std::runtime_error("Salvar novamente alterou o ID do bloco.");
            writeProject(testMapPath_);
            const auto example=mods_.parent_path().parent_path()/(mods_.parent_path().filename().wstring()+L"-exemplo");
            std::filesystem::create_directory(example);
            std::filesystem::copy(mods_,example/"content",
                std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks);
            const auto savedMap=map_.json();
            auto document=savedMap;
            document["editor_content"]="content";
            const auto exampleMap=example/"Farol de Cobre.json";
            game::atomicWrite(exampleMap,document.dump(2));
            openProject(exampleMap,true);
            if(!catalog_.blocks.contains(id) || map_.json()!=savedMap || mods_!=example/"content")
                throw std::runtime_error("O exemplo portátil perdeu o bloco.");
            std::fprintf(stderr,"Blockbench block test passed. MAP=%s\n",game::pathText(testMapPath_).c_str());
            std::fprintf(stderr,"EXAMPLE=%s\n",game::pathText(example).c_str());
            KillTimer(window_,blockbenchTestTimer);
            DestroyWindow(window_);
        }
    }

    std::filesystem::path settingsFile() const {
        return game::defaultUserDirectory(programDirectory_)/"editor-settings.json";
    }

    void readSettings() {
        try {
            const auto path=settingsFile();
            if(!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path)>16384) return;
            std::ifstream input(path);
            const auto config=nlohmann::json::parse(input);
            showGrid_=config.value("grid",true);start3d_=config.value("start_3d",false);
            cameraSpeed_=std::clamp(config.value("camera_speed",1.f),.5f,2.f);
            blockbench_=wide(config.value("blockbench",std::string{}));
            bridge_=wide(config.value("bridge",std::string{}));
        } catch(const std::exception&) {
            status_=L"Configurações inválidas. Usando os valores padrão.";
        }
    }

    void saveSettings() {
        const nlohmann::json config={{"grid",showGrid_},{"start_3d",start3d_},{"camera_speed",cameraSpeed_},
            {"blockbench",game::pathText(blockbench_)},{"bridge",game::pathText(bridge_)}};
        game::atomicWrite(settingsFile(),config.dump(2),true);
    }

    void changeSetting(int id) {
        if(id==Start3D) start3d_=!start3d_;
        if(id==GridToggle) showGrid_=!showGrid_;
        if(id==CameraSpeed) cameraSpeed_=cameraSpeed_>=2.f?.5f:cameraSpeed_+.5f;
        if(id==ChooseBlockbench || id==ChooseBridge) {
            const auto path=id==ChooseBlockbench?
                chooseFile(false,L"Selecione o Blockbench instalado",L"Blockbench\0Blockbench.exe\0"):
                chooseFile(false,L"Selecione o server.mjs da ponte",L"Ponte do Blockbench\0server.mjs\0");
            if(path.empty()) return;
            (id==ChooseBlockbench?blockbench_:bridge_)=path;
        }
        saveSettings();refresh(L"Configurações salvas.");
        updateSettingLabels();
    }

    void updateSettingLabels() {
        SetWindowTextW(GetDlgItem(window_,Start3D),start3d_?L"3D":L"Planta");
        SetWindowTextW(GetDlgItem(window_,GridToggle),showGrid_?L"Visível":L"Oculta");
        const auto speed=std::to_wstring(int(cameraSpeed_*100))+L"%";
        SetWindowTextW(GetDlgItem(window_,CameraSpeed),speed.c_str());
    }

    bool resolveBlockbench() {
        if(std::filesystem::is_regular_file(blockbench_)) return true;
        wchar_t local[32768]{};
        const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,DWORD(std::size(local)));
        if(count && count<std::size(local)) blockbench_=std::filesystem::path(local)/"Programs/Blockbench/Blockbench.exe";
        if(!std::filesystem::is_regular_file(blockbench_)) {
            blockbench_=chooseFile(false,L"Selecione o Blockbench instalado",L"Blockbench\0Blockbench.exe\0");
            if(blockbench_.empty()) return false;
        }
        saveSettings();return true;
    }

    std::filesystem::path copyContent(const std::string& label,const std::string& replacing={}) const {
        const auto work=game::createEditorRevision(label);
        const auto next=work/"content";
        constexpr auto options=std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks;
        if(replacing.empty()) std::filesystem::copy(mods_,next,options);
        else {
            std::filesystem::create_directory(next);
            for(const auto& entry:std::filesystem::directory_iterator(mods_)) {
                const auto name=entry.path().filename();
                if(name!=replacing) std::filesystem::copy(entry.path(),next/name,options);
            }
        }
        return next;
    }

    void recordContent(const std::filesystem::path& directory,std::wstring label) {
        attachDraftMap();
        Edit edit;edit.label=std::move(label);edit.content=std::pair{mods_,directory};
        history_.reserve(cursor_+1);apply(edit,true);record(std::move(edit));
    }

    void syncStudio() {
        const auto selected=SendMessageW(props_,CB_GETCURSEL,0,0);
        if(selected<=0) throw std::runtime_error("Escolha na lista o modelo do Studio que deseja atualizar");
        const auto modelId=modelIds_.at(size_t(selected-1));
        auto server=bridge_.empty()?programDirectory_.parent_path().parent_path()/"BLOCKBENCH/bridge/mcp/server.mjs":bridge_;
        if(!std::filesystem::is_regular_file(server)) server=chooseFile(false,L"Escolher o servidor do bridge local",L"Bridge do Blockbench\0server.mjs\0");
        if(server.empty()) return;
        const auto next=copyContent("sync");
        const auto work=next.parent_path();
        for(const auto& mod:game::discoverMods(next).mods) if(mod.id==game::AssetId(modelId).nameSpace()) {
            std::ifstream input(mod.root/"assets.json",std::ios::binary);
            const auto registry=nlohmann::json::parse(input);
            if(!registry.contains("studio")) break;
            for(const auto& [project,source]:registry.at("studio").items()) {
                const auto manifest=game::VirtualFileSystem::contained(mod.root,source.at("source").get<std::string>());
                std::ifstream manifestFile(manifest,std::ios::binary);
                const auto data=nlohmann::json::parse(manifestFile);
                bool matches=false;
                for(const auto& model:data.at("models")) if(model.at("id")==modelId) matches=true;
                if(!matches) continue;
                const auto plan=work/"sync.json";
                game::atomicWrite(plan,nlohmann::json{{"project",project},{"manifest",game::pathText(manifest)}}.dump());
                const auto result=game::runEditorProcess(L"node.exe",{(programDirectory_/"editor-studio.mjs").wstring(),server.wstring(),L"sync",plan.wstring()},work,{},{});
                if(result.exitCode) throw std::runtime_error(result.output.substr(0,1600));
                const auto verified=game::loadStudioPackage(manifest,work/"cache",mod.id);
                (void)verified;
                recordContent(next,L"Atualizar modelo · "+wide(modelId));refresh(L"Modelo atualizado a partir do Blockbench.");return;
            }
        }
        throw std::runtime_error("O modelo precisa pertencer a um pacote do Studio registrado pelo editor");
    }

    void testMap() {
        map_.validateDoors(catalog_);
        const auto spawn=map_.spawn();
        bool ground=false;
        for(int y=0;y<FiniteMap::height;++y) {
            const auto id=map_.block(spawn.x,y,spawn.z);
            if(!id.empty() && catalog_.blocks.at(std::string(id)).solid) ground=true;
        }
        if(!ground) throw std::runtime_error("Coloque um bloco sólido no ponto inicial antes de testar");
        const auto preview=std::filesystem::temp_directory_path()/L"ShardBoundEditor"/
            (std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        const auto mapPath=preview/L"preview.json";
        map_.save(mapPath);
        open(programDirectory_/L"game.exe",L"--test-map \""+mapPath.wstring()+L"\" --user-dir \""+
            (preview/L"user").wstring()+L"\" --mods \""+mods_.wstring()+L"\"");
        refresh(L"Teste iniciado no jogo.");
    }

    RECT viewport() const {
        auto area=previewViewport();InflateRect(&area,-25,-25);
        const auto b=map_.bounds();
        const int availableW=std::max(1,int(area.right-area.left)),availableH=std::max(1,int(area.bottom-area.top));
        const double scale=std::min(double(availableW)/b.width,double(availableH)/b.depth)*plantScale_;
        const int width=std::max(1,int(b.width*scale)),height=std::max(1,int(b.depth*scale));
        const auto x=area.left+(availableW-width)/2+LONG(plantX_),y=area.top+(availableH-height)/2+LONG(plantY_);
        return {x,y,x+width,y+height};
    }

    RECT previewViewport() const {
        RECT client{};GetClientRect(window_,&client);
        return {gutter,167,std::max<LONG>(gutter+1,client.right-inspector-gutter),
            std::max<LONG>(125,client.bottom-footer-37)};
    }

    RECT regionPanel() const {
        const auto area=previewViewport();
        const auto x=(area.left+area.right-429)/2,y=(area.top+area.bottom-220)/2;
        return {x,y,x+429,y+220};
    }

    void centerPreview() {
        const auto bounds=map_.bounds();centerX_=float(bounds.minX)+bounds.width*.5f;
        centerZ_=float(bounds.minZ)+bounds.depth*.5f;
        distance_=std::clamp(float(std::hypot(bounds.width,bounds.depth))*1.23f,12.f,900.f);
    }

    void zoom(int wheel) {
        if(page_!=Page::Map || regionCommand_) return;
        const float steps=float(wheel)/WHEEL_DELTA*cameraSpeed_;
        if(view3d_) distance_=std::clamp(distance_*std::pow(.90f,steps),3.f,950.f);
        else plantScale_=std::clamp(plantScale_*std::pow(1.13f,steps),.35f,8.f);
        refresh();
    }

    void panCamera(float x,float y) {
        if(!view3d_) {
            plantX_=std::clamp(plantX_+x,-8192.f,8192.f);plantY_=std::clamp(plantY_+y,-8192.f,8192.f);
            return;
        }
        const float amount=distance_*.002f;
        centerX_+=(-x*std::cos(orbit_)+y*std::sin(orbit_))*amount;
        centerZ_+=(x*std::sin(orbit_)+y*std::cos(orbit_))*amount;
    }

    void beginCameraGesture(bool left,POINT point) {
        const auto area=previewViewport();
        if(cameraGesture_ || page_!=Page::Map || regionCommand_ || !PtInRect(&area,point)) return;
        SetFocus(window_);SetCapture(window_);
        if(GetCapture()==window_) cameraGesture_=CameraGesture{point,point,left};
    }

    void updateCameraGesture(POINT point) {
        auto& gesture=*cameraGesture_;
        if(!gesture.moved) {
            if(std::abs(point.x-gesture.start.x)<std::max(3,GetSystemMetrics(SM_CXDRAG)) &&
                std::abs(point.y-gesture.start.y)<std::max(3,GetSystemMetrics(SM_CYDRAG))) return;
            gesture.moved=true;
        }
        const float dx=float(point.x-gesture.last.x)*cameraSpeed_,dy=float(point.y-gesture.last.y)*cameraSpeed_;
        gesture.last=point;
        if(dx==0 && dy==0) return;
        if(!gesture.left) {
            SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));
            panCamera(dx,dy);
        } else if(view3d_) {
            SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));
            orbit_=std::remainder(orbit_+dx*.009f,2*std::numbers::pi_v<float>);
            pitch_=std::clamp(pitch_+dy*.008f,.18f,1.50f);
        } else return;
        refresh();
    }

    void finishCameraGesture(bool left,POINT point) {
        if(!cameraGesture_ || cameraGesture_->left!=left) return;
        updateCameraGesture(point);
        const bool edit=left && !cameraGesture_->moved && page_==Page::Map && !regionCommand_;
        cancelCameraGesture();
        if(edit) paintCell(point.x,point.y);
    }

    void cancelCameraGesture() {
        if(!cameraGesture_) return;
        cameraGesture_.reset();
        if(GetCapture()==window_) ReleaseCapture();
        SetCursor(LoadCursorW(nullptr,IDC_ARROW));
    }

    void verifyCameraGestures() {
        const auto area=previewViewport();
        const POINT start{(area.left+area.right)/2,(area.top+area.bottom)/2};
        const auto point=MAKELPARAM(start.x,start.y);
        const auto moved=MAKELPARAM(start.x+40,start.y+24);
        const auto edits=history_.size();
        const auto drag=[&](UINT down,WPARAM button,UINT up) {
            SendMessageW(window_,down,button,point);
            SendMessageW(window_,WM_MOUSEMOVE,button,moved);
            SendMessageW(window_,up,0,moved);
        };
        const auto initialOrbit=orbit_,initialPitch=pitch_,initialX=centerX_,initialZ=centerZ_;
        drag(WM_LBUTTONDOWN,MK_LBUTTON,WM_LBUTTONUP);
        const auto rotation=std::remainder(orbit_-initialOrbit,2*std::numbers::pi_v<float>);
        if(rotation<=0 || pitch_==initialPitch || centerX_!=initialX || centerZ_!=initialZ || history_.size()!=edits)
            throw std::runtime_error("Dragging the left button to the right did not turn the camera to the right");
        const auto angle=orbit_,tilt=pitch_;
        drag(WM_RBUTTONDOWN,MK_RBUTTON,WM_RBUTTONUP);
        if((centerX_==initialX && centerZ_==initialZ) || orbit_!=angle || pitch_!=tilt || history_.size()!=edits)
            throw std::runtime_error("Right drag did not pan without editing");
        const auto distance=distance_;
        auto screen=start;ClientToScreen(window_,&screen);
        SendMessageW(window_,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(screen.x,screen.y));
        if(distance_>=distance) throw std::runtime_error("The wheel did not zoom in 3D");
        togglePreview();
        const auto x=plantX_,y=plantY_;
        drag(WM_RBUTTONDOWN,MK_RBUTTON,WM_RBUTTONUP);
        if(plantX_==x || plantY_==y || history_.size()!=edits)
            throw std::runtime_error("Right drag did not pan in the map plan");
        drag(WM_LBUTTONDOWN,MK_LBUTTON,WM_LBUTTONUP);
        if(history_.size()!=edits || orbit_!=angle || pitch_!=tilt)
            throw std::runtime_error("Dragging in the map plan edited the terrain");
    }

    void togglePreview() {
        cancelCameraGesture();
        if(view3d_) {view3d_=false;ShowWindow(renderSurface_,SW_HIDE);refresh();return;}
        if(!previewRenderer_) {
            const auto area=previewViewport();
            renderSurface_=child(L"STATIC",L"",WS_DISABLED,area.left,area.top,area.right-area.left,area.bottom-area.top);
            try {
                previewRenderer_=std::make_unique<game::Renderer>(renderSurface_,area.right-area.left,area.bottom-area.top,
                    programDirectory_/"assets/core/shaders/forward.hlsl",game::RenderConfig{false,1});
                previewRenderer_->scene({true,false,{.075f,.09f,.12f,1},{.84f,.79f,.68f,1},{.59f,.66f,.76f,1},1400,1800},{});
            } catch(...) {DestroyWindow(renderSurface_);renderSurface_=nullptr;throw;}
        }
        centerPreview();view3d_=true;
        if(!smoke_ && page_==Page::Map && !regionCommand_) {ShowWindow(renderSurface_,SW_SHOWNA);SetFocus(window_);}
        refresh();
    }

    game::Camera previewCamera() const {
        game::Camera camera;
        const float targetY=float(layer_)+.5f;
        camera.target={centerX_,targetY,centerZ_};
        const float radius=distance_*std::cos(pitch_);
        camera.eye={centerX_-std::sin(orbit_)*radius,targetY+distance_*std::sin(pitch_),centerZ_-std::cos(orbit_)*radius};
        camera.farPlane=std::max(240.f,distance_*3);
        return camera;
    }

    std::optional<POINT> previewCell(int x,int y) const {
        const auto area=previewViewport();const POINT pixel{x,y};
        if(!PtInRect(&area,pixel)) return std::nullopt;
        const auto camera=previewCamera();
        const float level=std::cos(pitch_),vertical=std::sin(pitch_);
        const float forwardX=std::sin(orbit_)*level,forwardY=-vertical,forwardZ=std::cos(orbit_)*level;
        const float rightX=std::cos(orbit_),rightZ=-std::sin(orbit_);
        const float sx=(2.f*(x-area.left)/(area.right-area.left)-1)*std::tan(camera.fov*.5f)*(area.right-area.left)/(area.bottom-area.top);
        const float sy=(1-2.f*(y-area.top)/(area.bottom-area.top))*std::tan(camera.fov*.5f);
        const float rayY=forwardY+sy*level;
        if(rayY>=-.001f) return std::nullopt;
        const float t=(float(layer_)+1-camera.eye.y)/rayY;
        const float px=camera.eye.x+t*(forwardX+sx*rightX+sy*vertical*std::sin(orbit_));
        const float pz=camera.eye.z+t*(forwardZ+sx*rightZ+sy*vertical*std::cos(orbit_));
        return POINT{LONG(std::floor(px)),LONG(std::floor(pz))};
    }

    void drawPreview() {
        if(IsIconic(window_)) return;
        if(previewCatalogDirty_) {
            previewMeshes_.clear();previewAssets_.reset();previewFiles_=game::VirtualFileSystem{};
            const auto cache=std::filesystem::temp_directory_path()/"ShardBoundEditor"/"textures";
            previewAssets_=std::make_unique<game::AssetManager>(previewFiles_,*previewRenderer_,cache);
            for(const auto& mod:game::discoverMods(mods_).mods) {
                previewFiles_.mount(mod.id,mod.root);
                std::ifstream input(mod.root/"assets.json",std::ios::binary);
                if(!input) continue;
                const auto registry=nlohmann::json::parse(input);
                if(registry.contains("textures")) for(const auto& [id,entry]:registry.at("textures").items()) {
                    const auto settings=entry.value("import",nlohmann::json::object());
                    previewAssets_->registerTexture({game::AssetId(id),entry.at("source").get<std::string>(),
                        settings.value("srgb",true),settings.value("filter",std::string("nearest"))=="nearest"});
                }
                if(registry.contains("models")) for(const auto& [id,entry]:registry.at("models").items())
                    previewAssets_->registerModel({game::AssetId(id),entry.at("source").get<std::string>()});
                if(registry.contains("studio")) for(const auto& [id,entry]:registry.at("studio").items())
                    previewAssets_->registerStudioPackage(game::AssetId(id),entry.at("source").get<std::string>());
            }
            previewCatalogDirty_=false;
        }
        if(previewDirty_) {
            std::vector<game::RenderItem> meshes;
            auto definition=catalog_;definition.maps.clear();
            definition.maps.emplace("editor:preview",std::make_shared<FiniteMap>(map_));
            const game::TerrainGenerator terrain(std::move(definition),"editor:preview","preview");
            const auto b=map_.bounds();
            std::map<std::string,game::ModelHandle> blockModels;
            for(auto z=game::floorDiv(b.minZ,16);z<=game::floorDiv(b.minZ+b.depth-1,16);++z) {
                for(auto x=game::floorDiv(b.minX,16);x<=game::floorDiv(b.minX+b.width-1,16);++x) {
                    const auto chunk=game::buildChunk(terrain,x,z);
                    game::Transform transform;transform.position={float(x*16),0,float(z*16)};
                for(const auto& [id,mesh]:chunk.meshes) {
                        game::Material material;
                        material.texture=previewAssets_->texture(game::AssetId(catalog_.blocks.at(id).texture));
                        meshes.push_back({previewRenderer_->createMesh(mesh.vertices,mesh.indices),material,game::transformMatrix(transform)});
                    }
                    for(const auto& placement:chunk.models) {
                        auto& model=blockModels[placement.id];
                        if(!model) model=previewAssets_->model(game::AssetId(placement.id));
                        auto position=transform;
                        position.position.x+=placement.position.x;
                        position.position.y+=placement.position.y;
                        position.position.z+=placement.position.z;
                        const auto matrix=game::transformMatrix(position);
                        for(auto part:model->parts) {part.world=game::multiply(part.world,matrix);meshes.push_back(std::move(part));}
                    }
                }
            }
            for(const auto& prop:map_.props()) {
                const auto model=previewAssets_->model(game::AssetId(prop.model));
                game::Transform transform;transform.position={float(prop.x)+.5f,float(prop.y),float(prop.z)+.5f};
                transform.rotation.y=prop.yaw*std::numbers::pi_v<float>/180;transform.scale={prop.scale,prop.scale,prop.scale};
                for(auto part:model->parts) {part.world=game::multiply(part.world,game::transformMatrix(transform));meshes.push_back(std::move(part));}
            }
            previewMeshes_=std::move(meshes);previewDirty_=false;
        }
        previewRenderer_->render(previewCamera(),previewMeshes_);previewRenderer_->present();
    }

    void paintCell(int mouseX,int mouseY) {
        const auto area=viewport();const POINT point{mouseX,mouseY};
        const auto view=previewViewport();
        if(!PtInRect(&view,point) || (!view3d_ && !PtInRect(&area,point))) return;
        SetFocus(window_);
        const auto b=map_.bounds();
        const auto cell=view3d_?previewCell(mouseX,mouseY):std::optional<POINT>{{b.minX+(mouseX-area.left)*b.width/(area.right-area.left),
            b.minZ+(mouseY-area.top)*b.depth/(area.bottom-area.top)}};
        if(!cell || !map_.contains(cell->x,cell->y)) return;
        const int x=cell->x,z=cell->y;
        if(selectingStructure_) {
            if(!structureCorner_) {structureCorner_=FiniteMap::Spawn{x,z};structureLayer_=layer_;refresh(L"Escolha a camada do topo e clique no canto oposto.");return;}
            registerSelection(x,z);return;
        }
        if(roomToLink_) {
            if(!entranceToLink_) {entranceToLink_=FiniteMap::Spawn{x,z};refresh(L"Clique onde o jogador volta ao sair da sala.");return;}
            Edit edit;edit.label=L"Ligar interior";
            edit.interior=FiniteMap::Interior{*entranceToLink_,{x,z},*roomToLink_->exit(),roomToLink_};
            history_.reserve(cursor_+1);
            auto staged=map_;staged.addInterior(*edit.interior);staged.validateDoors(catalog_);
            map_=std::move(staged);record(std::move(edit));roomToLink_.reset();entranceToLink_.reset();
            refresh(L"Interior ligado. Use Interagir junto à porta no jogo.");return;
        }
        if(GetKeyState(VK_CONTROL)<0) {
            Edit edit;edit.label=L"Saída do interior";
            edit.exit=std::pair{map_.exit(),std::optional{FiniteMap::Spawn{x,z}}};
            history_.reserve(cursor_+1);apply(edit,true);record(std::move(edit));refresh();return;
        }
        if(GetKeyState(VK_SHIFT)<0) {
            const auto old=map_.spawn();
            if(old.x==x && old.z==z) return;
            history_.reserve(cursor_+1);
            Edit edit;
            edit.spawn=std::pair{old,FiniteMap::Spawn{x,z}};
            edit.label=L"Ponto inicial";
            apply(edit,true);record(std::move(edit));
            refresh(L"Ponto inicial atualizado.");return;
        }
        const auto structure=SendMessageW(structures_,CB_GETCURSEL,0,0);
        if(structure>0) {placeStructure(structureIds_.at(size_t(structure-1)),x,z);return;}
        const auto model=SendMessageW(props_,CB_GETCURSEL,0,0);
        if(model>0) {
            wchar_t text[32]{};GetWindowTextW(propScale_,text,32);
            std::wstring value=text;std::replace(value.begin(),value.end(),L',',L'.');
            size_t end=0;const auto scale=std::stof(value,&end);
            if(end!=value.size()) throw std::runtime_error("Informe a escala do modelo");
            Edit edit;edit.label=L"Colocar modelo";
            edit.prop=FiniteMap::Prop{modelIds_.at(size_t(model-1)),x,layer_,z,float(SendMessageW(rotation_,CB_GETCURSEL,0,0)*90),scale};
            history_.reserve(cursor_+1);apply(edit,true);record(std::move(edit));refresh();return;
        }
        const auto selected=SendMessageW(palette_,LB_GETCURSEL,0,0);
        if(terrainTool_==TerrainTool::Pick) {
            auto id=map_.block(x,layer_,z);
            for(int y=layer_-1;id.empty() && y>=0;--y) id=map_.block(x,y,z);
            if(id.empty()) {refresh(L"Nenhum bloco nesta coluna.");return;}
            const auto name=wide(id);
            SetWindowTextW(filter_,name.c_str());panel_=MapPanel::Library;layout();
            selectTerrainTool(TerrainTool::Brush);refresh(L"Bloco selecionado.");return;
        }
        const bool erase=terrainTool_==TerrainTool::Erase;
        if(!erase && selected==LB_ERR) return;
        const std::string next=erase?std::string{}:ids_.at(visibleIds_.at(size_t(selected)));
        if(!erase && terrainTool_==TerrainTool::Fill) {
            if(!fillCorner_) {fillCorner_=FiniteMap::Spawn{x,z};refresh(L"Clique no canto oposto para preencher a camada atual.");return;}
            const auto first=*fillCorner_;
            Edit edit;edit.label=L"Preencher região";
            for(int row=std::min(first.z,z);row<=std::max(first.z,z);++row)
                for(int col=std::min(first.x,x);col<=std::max(first.x,x);++col) {
                    const auto old=map_.block(col,layer_,row);
                    if(old!=next) edit.cells.push_back({col,layer_,row,std::string(old),next});
                }
            fillCorner_.reset();
            if(!edit.cells.empty()) {history_.reserve(cursor_+1);apply(edit,true);record(std::move(edit));}
            refresh(L"Região preenchida na camada atual.");return;
        }
        const auto prior=map_.block(x,layer_,z);
        if(prior==next) return;
        Edit edit;edit.cells.push_back({x,layer_,z,std::string(prior),next});edit.label=erase?L"Apagar bloco":blockNameFor(next);
        history_.reserve(cursor_+1);
        map_.setBlock(x,layer_,z,next,catalog_);
        record(std::move(edit));
        refresh(L"X "+std::to_wstring(x)+L"   Z "+std::to_wstring(z));
    }

    void selectTerrainTool(TerrainTool tool) {
        terrainTool_=tool;fillCorner_.reset();selectingStructure_=false;roomToLink_.reset();entranceToLink_.reset();
        SendMessageW(structures_,CB_SETCURSEL,0,0);SendMessageW(props_,CB_SETCURSEL,0,0);
        const wchar_t* guidance[]={L"",L"Clique para apagar na camada atual.",L"Clique no mapa para escolher um bloco existente.",
            L"Clique no primeiro canto da região."};
        refresh(guidance[UINT(tool)]);
    }

    void apply(const Edit& edit,bool forward) {
        previewDirty_=true;
        if(edit.content) useContent(forward?edit.content->second:edit.content->first);
        game::applyEditorMapEdit(map_,catalog_,edit,forward);
        if(edit.revision) centerPreview();
    }

    void placeStructure(const std::string& id,int x,int z) {
        const int turns=int(SendMessageW(rotation_,CB_GETCURSEL,0,0));
        Edit edit;
        edit.label=L"Colocar "+wide(id);
        for(const auto& cell:catalog_.structures.at(id).cells) {
            int dx=cell.x,dz=cell.z;
            for(int turn=0;turn<turns;++turn) { const int oldX=dx;dx=-dz;dz=oldX; }
            const int bx=x+dx,by=layer_+cell.y,bz=z+dz;
            if(!map_.contains(bx,bz) || by>=FiniteMap::height) throw std::runtime_error("A estrutura ultrapassa os limites do mapa");
            const auto before=map_.block(bx,by,bz);
            if(before!=cell.block) edit.cells.push_back({bx,by,bz,std::string(before),cell.block});
        }
        if(edit.cells.empty()) return;
        history_.reserve(cursor_+1);
        map_.placeStructures(nlohmann::json::array({{id,x,layer_,z,turns*90}}),catalog_);
        record(std::move(edit));refresh(L"Estrutura colocada.");
    }

    void registerSelection(int x,int z) {
        const int x0=std::min(structureCorner_->x,x),x1=std::max(structureCorner_->x,x);
        const int z0=std::min(structureCorner_->z,z),z1=std::max(structureCorner_->z,z);
        const int y0=std::min(structureLayer_,layer_),y1=std::max(structureLayer_,layer_);
        if(x1-x0>12 || z1-z0>12 || y1-y0>15) throw std::runtime_error("Selecione até 13 × 16 × 13 blocos");
        if(catalog_.structures.size()>=64) throw std::runtime_error("O catálogo atingiu o limite de estruturas");
        const auto target=chooseFile(true,L"Escolher ID do novo mod de estrutura (ex.: torre.json)");
        if(target.empty()) return;
        const auto mod=target.stem().string();
        if(!game::validNamespace(mod) || mod=="core" || mod.size()>48) throw std::runtime_error("ID de mod inválido");
        for(const auto& existing:game::discoverMods(mods_).mods) if(existing.id==mod) throw std::runtime_error("O ID do mod já existe");
        const auto next=copyContent("structure");
        const auto destination=next/mod;
        if(std::filesystem::exists(destination)) throw std::runtime_error("A pasta do mod já existe");
        using Json=nlohmann::json;
        auto cells=Json::array();std::set<std::string> dependencies;
        for(int y=y0;y<=y1;++y) for(int zz=z0;zz<=z1;++zz) for(int xx=x0;xx<=x1;++xx) {
            const auto block=map_.block(xx,y,zz);
            if(block.empty()) continue;
            if(cells.size()==512) throw std::runtime_error("A seleção contém mais de 512 blocos");
            dependencies.insert(game::AssetId(std::string(block)).nameSpace());
            cells.push_back({xx-(x0+x1)/2,y-y0,zz-(z0+z1)/2,block});
        }
        if(cells.empty()) throw std::runtime_error("A seleção está vazia");
        const auto stage=next/(".editor-structure-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        if(!std::filesystem::create_directory(stage)) throw std::runtime_error("Não foi possível preparar a estrutura");
        const auto id=mod+":structure";dependencies.erase("core");
        try {
            game::atomicWrite(stage/"world.json",Json{{"version",1},{"structures",{{id,cells}}}}.dump(2));
            game::atomicWrite(stage/"mod.json",Json{{"id",mod},{"name",mod},{"api","0.1"},{"version","1.0.0"},{"dependencies",dependencies}}.dump(2));
            game::renameDirectory(stage,destination);
        } catch(...) {
            std::error_code ignored;std::filesystem::remove(stage/"mod.json",ignored);std::filesystem::remove(stage/"world.json",ignored);
            std::filesystem::remove(stage,ignored);throw;
        }
        recordContent(next,L"Registrar estrutura · "+wide(id));selectingStructure_=false;structureCorner_.reset();
        refresh(L"Estrutura registrada na biblioteca.");
    }

    void record(Edit edit) {
        previewDirty_=true;
        if(cursor_<history_.size()) needsSave_=true;
        for(size_t i=history_.size();i>cursor_;--i) SendMessageW(timeline_,LB_DELETESTRING,i,0);
        history_.resize(cursor_);history_.push_back(std::move(edit));++cursor_;
        SendMessageW(timeline_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(history_.back().label.c_str()));
    }

    void jumpToEdit() {
        const auto row=SendMessageW(timeline_,LB_GETCURSEL,0,0);
        if(row==LB_ERR || size_t(row)>history_.size()) return;
        const auto target=size_t(row);
        while(cursor_>target) {apply(history_[cursor_-1],false);--cursor_;}
        while(cursor_<target) {apply(history_[cursor_],true);++cursor_;}
        refresh();
    }

    void verifyTimeline() {
        const auto folder=std::filesystem::temp_directory_path()/"ShardBoundEditor"/
            ("timeline-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        std::filesystem::create_directories(folder);
        const auto staged=folder/"content";
        std::filesystem::copy(mods_,staged,std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks);
        game::AuthoredBlock block;block.mod="timeline_probe";block.pixels.fill(0xff9922);game::publishBlock(staged,block);
        const auto original=std::make_shared<const FiniteMap>(map_);
        auto changed=map_;const auto spawn=map_.spawn();
        changed.setBlock(spawn.x,20,spawn.z,"timeline_probe:block",WorldDefinition::load(game::discoverMods(staged).mods));
        Edit edit;edit.label=L"Terminal";edit.content=std::pair{mods_,staged};
        edit.revision=std::pair{original,std::make_shared<const FiniteMap>(std::move(changed))};
        apply(edit,true);record(std::move(edit));
        if(!catalog_.blocks.contains("timeline_probe:block") || map_.block(spawn.x,20,spawn.z)!="timeline_probe:block")
            throw std::runtime_error("Timeline failed to apply the mod and map revisions");
        SendMessageW(timeline_,LB_SETCURSEL,0,0);jumpToEdit();
        if(catalog_.blocks.contains("timeline_probe:block") || map_.block(spawn.x,20,spawn.z)!=original->block(spawn.x,20,spawn.z))
            throw std::runtime_error("Timeline failed to restore the mod and map revisions");
        SendMessageW(timeline_,LB_SETCURSEL,1,0);jumpToEdit();
        if(!catalog_.blocks.contains("timeline_probe:block") || map_.block(spawn.x,20,spawn.z)!="timeline_probe:block")
            throw std::runtime_error("Timeline failed to reapply the mod and map revisions");
        Edit painting;painting.label=L"Pintar chão";
        painting.cells.push_back({spawn.x,21,spawn.z,std::string(map_.block(spawn.x,21,spawn.z)),"timeline_probe:block"});
        apply(painting,true);record(std::move(painting));
        Edit start;start.label=L"Ponto inicial";
        start.spawn=std::pair{spawn,FiniteMap::Spawn{spawn.x+1,spawn.z}};
        apply(start,true);record(std::move(start));
        Edit model;model.label=L"Colocar modelo";
        model.prop=FiniteMap::Prop{"foundation:sword",spawn.x,21,spawn.z,0,1};
        apply(model,true);record(std::move(model));
        SendMessageW(timeline_,LB_SETCURSEL,2,0);jumpToEdit();
        const auto saved=folder/"saved.json";
        writeProject(saved);
        SendMessageW(timeline_,LB_SETCURSEL,0,0);jumpToEdit();
        replace(*original,{},false);
        openProject(saved);
        if(dirty() || cursor_!=2 || history_.size()!=4 || history_[1].label!=L"Pintar chão" ||
            SendMessageW(timeline_,LB_GETCURSEL,0,0)!=2 || mods_!=staged ||
            map_.block(spawn.x,21,spawn.z)!="timeline_probe:block" || map_.spawn()!=spawn)
            throw std::runtime_error("Saved map failed to restore its timeline and selected revision");
        SendMessageW(timeline_,LB_SETCURSEL,0,0);jumpToEdit();
        if(cursor_!=0 || catalog_.blocks.contains("timeline_probe:block") || map_.json()!=original->json())
            throw std::runtime_error("Restored timeline failed to return to its initial revision");
        SendMessageW(timeline_,LB_SETCURSEL,4,0);jumpToEdit();
        if(cursor_!=4 || map_.spawn()==spawn || map_.props().size()!=original->props().size()+1 || mods_!=staged)
            throw std::runtime_error("Restored timeline failed to redo revisions beyond the saved position");
        writeProject(saved);
        openProject(std::filesystem::path(saved.wstring()+L".bak"));
        if(cursor_!=2 || history_.size()!=4)
            throw std::runtime_error("The project backup lost the previous timeline");

        const auto incomplete=folder/"missing-timeline.json";
        auto mapOnly=map_.json();
        mapOnly["editor_content"]=game::pathText(mods_);
        mapOnly["editor_timeline"]=".shardbound-timeline-00000000-0000-0000-0000-000000000000.json";
        game::atomicWrite(incomplete,mapOnly.dump());
        const auto expectedMap=map_.json();
        openProject(incomplete);
        if(!history_.empty() || map_.json()!=expectedMap || mods_!=staged)
            throw std::runtime_error("Missing timeline prevented recovery of the saved map");

        auto old=original->json();
        const auto legacy=folder/"old.json";
        game::atomicWrite(legacy,old.dump());
        openProject(legacy);
        if(!history_.empty() || dirty() || map_.json()!=old)
            throw std::runtime_error("A legacy map no longer opens without a timeline");

        if(!original->interiors().empty()) {
            auto withoutRoom=*original;
            const auto room=withoutRoom.interiors().back();
            withoutRoom.removeInterior();
            replace(std::move(withoutRoom),{},false);
            Edit door;door.label=L"Ligar interior";door.interior=room;
            apply(door,true);record(std::move(door));
            const auto doors=folder/"doors.json";
            writeProject(doors);
            openProject(doors);
            command(Undo);
            if(map_.interiors().size()+1!=original->interiors().size())
                throw std::runtime_error("Reopened timeline could not undo the room link");
            command(Redo);
            if(map_.json()!=old) throw std::runtime_error("Reopened timeline could not redo the room link");
        }
    }

    void verifyTabs() {
        auto* first=loadedMap_;
        command(NewTab);
        if(page_!=Page::Home || tabs_.size()!=3 || currentTab_==tabs_.front().get())
            throw std::runtime_error("The plus button did not create a new launcher tab");
        command(HomeNew);
        command(ApplyRegion);
        auto* edited=loadedMap_;
        if(edited==first || tabs_.size()!=3 || !dirty()) throw std::runtime_error("A new map did not replace its launcher");
        const auto spawn=map_.spawn();
        Edit edit;edit.label=L"Teste de abas";
        edit.cells.push_back({spawn.x,20,spawn.z,std::string(map_.block(spawn.x,20,spawn.z)),"plains:grass"});
        apply(edit,true);record(std::move(edit));
        layer_=20;plantScale_=1.8f;panel_=MapPanel::History;
        SetWindowTextW(filter_,L"Grama");
        command(tabSelectBase+first->slot);
        if(loadedMap_!=first || layer_==20 || map_.block(spawn.x,20,spawn.z)=="plains:grass" ||
            controlText(filter_)==L"Grama" || !tabDirty(*edited))
            throw std::runtime_error("Tab switch leaked state from another map");
        command(tabSelectBase+edited->slot);
        if(loadedMap_!=edited || cursor_!=1 || history_.front().label!=L"Teste de abas" ||
            layer_!=20 || plantScale_!=1.8f || panel_!=MapPanel::History || controlText(filter_)!=L"Grama" ||
            map_.block(spawn.x,20,spawn.z)!="plains:grass" || SendMessageW(timeline_,LB_GETCURSEL,0,0)!=1)
            throw std::runtime_error("Tab switch failed to restore its map and editor state");
        const auto savedPath=first->map->path;
        openProject(savedPath,true);
        if(loadedMap_!=first || tabs_.size()!=3) throw std::runtime_error("A project opened twice");
        moveTab(edited->slot,{tabsLeft,tabsTop+5});
        if(tabs_[1].get()!=edited || loadedMap_!=edited) throw std::runtime_error("Tab could not be reordered");
        command(CycleTabs);
        if(loadedMap_!=first) throw std::runtime_error("Tab cycling did not follow the new order");
        command(CloseTab);
        if(loadedMap_!=edited || !dirty() || tabs_.size()!=2) throw std::runtime_error("Closing a map lost another tab");
    }

    void drawWorkspace(HDC dc,const RECT& client) const {
        const int right=int(client.right),bottom=int(client.bottom)-footer,side=right-inspector;
        theme::fill(dc,client,theme::background);
        theme::fill(dc,{0,0,right,toolbar},theme::panel);
        theme::fill(dc,{0,toolbar-1,right,toolbar},theme::line);
        theme::text(dc,{18,20,150,59},L"ShardBound",theme::ink,theme::font(19,FW_SEMIBOLD));
        theme::fill(dc,{0,bottom,right,int(client.bottom)},theme::panel);
        theme::text(dc,{18,bottom+1,right-18,int(client.bottom)},status_,theme::muted,theme::font(13));
        if(page_==Page::BlockModel) {
            theme::text(dc,{18,77,73,109},L"Nome");
            const auto state=currentTab_->block->pane->status();
            if(!state.empty()) theme::text(dc,{24,136,right-24,181},state,theme::muted,theme::font(16));
            return;
        }
        if(page_==Page::Blockbench) {
            theme::text(dc,{18,76,right-130,105},L"Modelagem  /  Blockbench",theme::ink,theme::font(16,FW_SEMIBOLD));
            if(blockbenchPane_) {
                const auto state=blockbenchPane_->status();
                if(!state.empty()) theme::text(dc,{24,136,right-24,181},state,theme::muted,theme::font(16));
            }
            return;
        }
        if(page_!=Page::Map) {drawPage(dc,right);return;}
        theme::fill(dc,{side,115,right,bottom},theme::panel);
        theme::fill(dc,{side,115,side+1,bottom},theme::line);
        const auto view=previewViewport();
        theme::fill(dc,view,theme::canvas);
        if(!view3d_) for(int x=view.left+12;x<view.right;x+=24)
            for(int y=view.top+12;y<view.bottom;y+=24) SetPixelV(dc,x,y,theme::line);
        const auto title=path_.empty()?L"Mapa em edição":path_.stem().wstring();
        theme::text(dc,{500,78,right-169,110},title+(dirty()?L"  •":L""),theme::ink,theme::font(15,FW_SEMIBOLD));
        theme::text(dc,{side-gutter-81,124,side-gutter-32,156},std::to_wstring(layer_)+L"/47",theme::ink,theme::font(13,FW_SEMIBOLD),
            DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        const auto help=view3d_?L"Arraste com o esquerdo para girar · Arraste com o direito para mover · Roda para zoom":
            L"Arraste com o direito para mover · Roda para zoom · Shift + clique marca o início";
        theme::text(dc,{view.left,bottom-34,view.right,bottom-4},help,theme::muted,theme::font(12));
        if(panel_==MapPanel::Library) {
            theme::text(dc,{side+14,169,right-157,203},L"Paleta",theme::ink,theme::font(20,FW_SEMIBOLD));
            const auto count=visibleIds_.empty()?L"Nenhum bloco encontrado":std::to_wstring(visibleIds_.size())+L" blocos";
            theme::text(dc,{side+14,bottom-78,right-14,bottom-46},count,theme::muted,theme::font(12));
        } else if(panel_==MapPanel::Placement) {
            theme::text(dc,{side+14,184,right-14,209},L"Estrutura",theme::muted);
            theme::text(dc,{side+14,245,right-14,270},L"Modelo",theme::muted);
            theme::text(dc,{side+14,307,side+138,331},L"Rotação",theme::muted);
            theme::text(dc,{side+154,307,right-14,331},L"Escala do modelo",theme::muted);
        } else {
            theme::text(dc,{side+14,172,right-62,207},L"Linha do tempo",theme::ink,theme::font(19,FW_SEMIBOLD));
            theme::text(dc,{right-62,172,right-14,207},std::to_wstring(cursor_)+L" / "+std::to_wstring(history_.size()),
                theme::muted,theme::font(12),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        }
    }

    void drawPage(HDC dc,int right) const {
        const int x=std::max(28,(right-1110)/2),w=std::min(1110,right-2*x);
        const auto heading=[&](int y,std::wstring_view title) {
            theme::text(dc,{x,y,x+w,y+60},title,theme::ink,theme::font(35,FW_SEMIBOLD));
        };
        const auto copy=[&](RECT box,std::wstring_view value) {
            theme::text(dc,box,value,theme::muted,theme::font(15),DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        };
        if(page_==Page::Home) {
            heading(93,L"O que você quer criar?");
            theme::fill(dc,{x,170,x+w,380},theme::panel);
            theme::fill(dc,{x,170,x+3,380},theme::accent);
            const int split=x+w*63/100;
            theme::text(dc,{x+24,186,split-15,228},L"Um mundo para explorar",theme::ink,theme::font(25,FW_SEMIBOLD));
            copy({x+24,232,split-15,258},L"Crie um mapa, abra um projeto ou volte ao mundo atual.");
            const RECT thumbnail{split,185,x+w-16,365};
            theme::fill(dc,thumbnail,theme::canvas);
            if(!catalog_.maps.empty()) {
                const int saved=SaveDC(dc);
                IntersectClipRect(dc,thumbnail.left,thumbnail.top,thumbnail.right,thumbnail.bottom);
                drawMapThumbnail(dc,thumbnail,*catalog_.maps.begin()->second);
                RestoreDC(dc,saved);
            }
            const int card=(w-48)/5;
            const std::array labels{L"Crie blocos, pinte texturas e importe modelos.",
                L"Monte armas, armaduras e sequências de golpes.",
                L"Modele, pinte e anime em 3D.",
                L"Descreva uma tarefa e revise as mudanças.",
                L"Ajuste a câmera e os caminhos dos aplicativos."};
            for(int i=0;i<int(labels.size());++i) {
                const int left=x+i*(card+12);
                theme::fill(dc,{left,408,left+card,542},theme::panel);
                copy({left+12,424,left+card-12,484},labels[i]);
            }
            return;
        }
        if(page_==Page::Settings) {
            heading(93,L"Configurações");
            theme::fill(dc,{x,162,x+w,352},theme::panel);
            theme::fill(dc,{x,370,x+w,561},theme::panel);
            theme::text(dc,{x+20,166,x+320,200},L"Editor de mapas",theme::ink,theme::font(19,FW_SEMIBOLD));
            theme::text(dc,{x+20,202,x+320,236},L"Vista ao abrir o mapa");
            theme::text(dc,{x+20,248,x+320,282},L"Grade na planta");
            theme::text(dc,{x+20,294,x+320,328},L"Velocidade da câmera");
            theme::text(dc,{x+20,372,x+320,405},L"Blockbench",theme::ink,theme::font(19,FW_SEMIBOLD));
            theme::text(dc,{x+20,406,x+325,440},blockbench_.empty()?L"Localização automática":blockbench_.wstring());
            theme::text(dc,{x+20,456,x+325,490},bridge_.empty()?L"Ponte local (server.mjs)":bridge_.wstring());
            theme::text(dc,{x+20,506,x+325,540},L"Plugins da versão Desktop");
            return;
        }
        const auto title=page_==Page::Blocks?L"Blocos":page_==Page::Gear?L"Equipamentos":L"Assistente IA";
        heading(93,title);
        theme::fill(dc,{x,170,x+w,335},theme::panel);
        const auto instruction=page_==Page::Blocks?L"Crie e pinte blocos no Blockbench. O bloco salvo aparece na paleta do mapa.":
            page_==Page::Gear?L"Escolha o tipo, importe o modelo e configure os atributos do equipamento.":
            L"O assistente trabalha no mapa atual. Escolha a conexão, descreva a tarefa e revise o rascunho no final.";
        copy({x+24,206,x+w-24,253},instruction);
    }

    void drawMapThumbnail(HDC dc,RECT area,const FiniteMap& map) const {
        const auto bounds=map.bounds();
        InflateRect(&area,-10,-10);
        const auto px=double(area.right-area.left)/(bounds.width+bounds.depth);
        const auto py=double(area.bottom-area.top)/(bounds.width+bounds.depth);
        const double horizontal=std::min(px,py*3)*.94,vertical=horizontal*.34;
        const double cx=(area.left+area.right)*.5,cy=(area.top+area.bottom)*.5-
            (bounds.width+bounds.depth)*vertical*.5+9;
        const auto brush=SelectObject(dc,GetStockObject(DC_BRUSH)),pen=SelectObject(dc,GetStockObject(NULL_PEN));
        const int skip=std::max(1,std::max(bounds.width,bounds.depth)/96);
        for(int z=0;z<bounds.depth;z+=skip) for(int x=0;x<bounds.width;x+=skip) {
            int y=FiniteMap::height-1;
            auto id=map.block(bounds.minX+x,y,bounds.minZ+z);
            while(id.empty() && y>0) id=map.block(bounds.minX+x,--y,bounds.minZ+z);
            if(id.empty()) continue;
            const double center=cx+(x-z-(bounds.width-bounds.depth)*.5)*horizontal;
            const double top=cy+(x+z)*vertical-y*1.35;
            const LONG w=std::max(1L,LONG(std::ceil(horizontal*skip))),h=std::max(1L,LONG(std::ceil(vertical*skip)));
            const LONG sx=LONG(center),sy=LONG(top);
            const POINT tile[]={{sx,sy},{sx+w,sy+h},{sx,sy+2*h},{sx-w,sy+h}};
            SetDCBrushColor(dc,blockColor(id));Polygon(dc,tile,4);
        }
        SelectObject(dc,pen);SelectObject(dc,brush);
    }
    void drawRegion(HDC dc) const {
        const auto area=regionPanel();
        theme::fill(dc,{area.left+3,area.top+5,area.right+3,area.bottom+5},RGB(10,12,16));
        theme::fill(dc,area,theme::panel);
        SetDCBrushColor(dc,theme::line);FrameRect(dc,&area,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        theme::text(dc,{area.left+18,area.top+13,area.right-18,area.top+50},
            regionCommand_==NewMap?L"Novo mapa":L"Importar região",theme::ink,theme::font(22,FW_SEMIBOLD));
        theme::text(dc,{area.left+18,area.top+54,area.right-18,area.top+79},
            regionCommand_==NewMap?L"Defina a origem e o tamanho do mapa.":L"Informe a área que quer importar do save.",theme::muted);
        constexpr std::array labels{L"Origem X",L"Origem Z",L"Largura",L"Profundidade"};
        for(int i=0;i<4;++i) theme::text(dc,{area.left+18+i*101,area.top+89,area.left+109+i*101,area.top+111},labels[i],theme::muted);
    }

    void draw() {
        PAINTSTRUCT paint{};const auto screen=BeginPaint(window_,&paint);
        RECT client{};GetClientRect(window_,&client);
        const auto buffer=CreateCompatibleDC(screen);
        const auto bitmap=CreateCompatibleBitmap(screen,std::max(1,int(client.right)),std::max(1,int(client.bottom)));
        const auto old=buffer && bitmap?SelectObject(buffer,bitmap):nullptr;
        const auto dc=old?buffer:screen;
        drawWorkspace(dc,client);
        if(page_==Page::Map && !view3d_) {
            const auto view=previewViewport();const int saved=SaveDC(dc);
            IntersectClipRect(dc,view.left,view.top,view.right,view.bottom);
            drawMap(dc);
            RestoreDC(dc,saved);
        }
        if(regionCommand_) drawRegion(dc);
        if(old) {
            BitBlt(screen,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old);
        }
        if(bitmap) DeleteObject(bitmap);
        if(buffer) DeleteDC(buffer);
        EndPaint(window_,&paint);
        if(page_==Page::Map && view3d_) drawPreview();
    }

    void drawMap(HDC dc) const {
        const auto area=viewport();const auto b=map_.bounds();
        std::vector<DWORD> pixels(size_t(b.width)*b.depth);
        for(int z=0;z<b.depth;++z) for(int x=0;x<b.width;++x) {
            auto color=theme::canvas;
            for(int y=layer_;y>=0;--y) {
                const auto id=map_.block(x+b.minX,y,z+b.minZ);
                if(id.empty()) continue;
                const auto found=colors_.find(id);
                if(found!=colors_.end()) color=found->second;
                const int shade=int(game::coordinateHash(21,x+b.minX,z+b.minZ)%15)-7;
                const auto channel=[&](int n) {return std::clamp(n+shade,0,255);};
                color=RGB(channel(GetRValue(color)),channel(GetGValue(color)),channel(GetBValue(color)));
                if(y!=layer_) color=RGB(GetRValue(color)*3/5,GetGValue(color)*3/5,GetBValue(color)*3/5);
                break;
            }
            pixels[size_t(z)*b.width+x]=(DWORD(GetRValue(color))<<16)|(DWORD(GetGValue(color))<<8)|GetBValue(color);
        }
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=b.width;
        info.bmiHeader.biHeight=-b.depth;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,area.left,area.top,area.right-area.left,area.bottom-area.top,0,0,b.width,b.depth,
            pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
        const auto gridPen=CreatePen(PS_SOLID,1,RGB(92,111,102));
        if(gridPen) {
            const auto previous=SelectObject(dc,gridPen);
            const auto chunkLine=[](int first) {return (16-first%16)%16;};
            if(showGrid_ && (area.right-area.left)/b.width>=3 && (area.bottom-area.top)/b.depth>=3) {
                for(int cell=chunkLine(b.minX);cell<b.width;cell+=16) {
                    const int pos=area.left+cell*(area.right-area.left)/b.width;
                    MoveToEx(dc,pos,area.top,nullptr);LineTo(dc,pos,area.bottom);
                }
                for(int cell=chunkLine(b.minZ);cell<b.depth;cell+=16) {
                    const int pos=area.top+cell*(area.bottom-area.top)/b.depth;
                    MoveToEx(dc,area.left,pos,nullptr);LineTo(dc,area.right,pos);
                }
            }
            SelectObject(dc,previous);DeleteObject(gridPen);
        }
        const auto spawn=map_.spawn();
        const auto x=area.left+((spawn.x-b.minX)*2+1)*(area.right-area.left)/(2*b.width);
        const auto z=area.top+((spawn.z-b.minZ)*2+1)*(area.bottom-area.top)/(2*b.depth);
        theme::fill(dc,{x-5,z-5,x+5,z+5},theme::canvas);
        theme::fill(dc,{x-3,z-3,x+3,z+3},theme::ink);
        const auto doorBrush=CreateSolidBrush(RGB(248,149,60));
        const auto drawDoor=[&](FiniteMap::Spawn door) {
            const auto dx=area.left+((door.x-b.minX)*2+1)*(area.right-area.left)/(2*b.width);
            const auto dz=area.top+((door.z-b.minZ)*2+1)*(area.bottom-area.top)/(2*b.depth);
            const RECT rectangle{dx-3,dz-3,dx+4,dz+4};FrameRect(dc,&rectangle,doorBrush);
        };
        if(map_.exit()) drawDoor(*map_.exit());
        for(const auto& room:map_.interiors()) drawDoor(room.entrance);
        if(fillCorner_) drawDoor(*fillCorner_);
        DeleteObject(doorBrush);
        for(const auto& prop:map_.props()) {
            const auto px=area.left+((prop.x-b.minX)*2+1)*(area.right-area.left)/(2*b.width);
            const auto pz=area.top+((prop.z-b.minZ)*2+1)*(area.bottom-area.top)/(2*b.depth);
            const RECT propMarker{px-2,pz-2,px+3,pz+3};FillRect(dc,&propMarker,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        }
    }
};

LRESULT CALLBACK dispatch(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(message==WM_NCCREATE) {
        const auto create=reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    const auto editor=reinterpret_cast<Editor*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(!editor) return DefWindowProcW(window,message,wParam,lParam);
    try { return editor->handle(window,message,wParam,lParam); }
    catch(const std::exception& error) {
        if(message==WM_APP+1 || message==WM_APP+3 || message==WM_APP+4) {
            std::fprintf(stderr,"%s\n",error.what());DestroyWindow(window);PostQuitMessage(1);return 0;
        }
        const auto messageText=wide(error.what());
        MessageBoxW(window,messageText.c_str(),L"ShardBound Editor",MB_ICONERROR);
        if(message==WM_CREATE) return -1;
        if(message==WM_PAINT) ValidateRect(window,nullptr);
        return 0;
    }
}
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    try {
        const auto apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(FAILED(apartment)) throw std::runtime_error("Não foi possível inicializar a interface do editor");
        struct Apartment {~Apartment() {CoUninitialize();}} com;
        SetProcessDPIAware();
        int argc=0;
        std::unique_ptr<wchar_t*,decltype(&LocalFree)> arguments(CommandLineToArgvW(GetCommandLineW(),&argc),LocalFree);
        if(!arguments) throw std::runtime_error("Não foi possível ler os argumentos do editor");
        const auto argv=arguments.get();
        const bool blockbenchTest=argc==2 && std::wstring_view(argv[1])==L"--smoke-blockbench";
        const bool smoke=blockbenchTest || (argc==2 && std::wstring_view(argv[1])==L"--smoke");
        if(!smoke && argc!=1 && (argc!=3 || std::wstring_view(argv[1])!=L"--mods")) throw std::runtime_error("Uso: shardbound_editor [--mods DIRETORIO]");
        std::array<wchar_t,32768> executable{};
        const auto count=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        if(!count || count>=executable.size()) throw std::runtime_error("Cannot locate editor executable");
        const auto programDirectory=std::filesystem::path(executable.data()).parent_path();
        const auto directory=argc==3?std::filesystem::absolute(argv[2]):programDirectory/"content";
        auto discovery=game::discoverMods(directory);
        for(const auto& diagnostic:discovery.diagnostics) if(diagnostic.code!="MOD106") throw game::DiagnosticError(diagnostic);
        Editor editor(WorldDefinition::load(discovery.mods),programDirectory,directory,smoke,blockbenchTest);

        WNDCLASSW type{};type.lpfnWndProc=dispatch;type.hInstance=GetModuleHandleW(nullptr);
        type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        type.lpszClassName=L"ShardBoundMapEditor";
        if(!RegisterClassW(&type)) throw std::runtime_error("Cannot register editor window");
        RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
        const int width=std::clamp(int(work.right-work.left)-48,1040,1440);
        const int height=std::clamp(int(work.bottom-work.top)-34,690,930);
        const int x=work.left+std::max(0,int(work.right-work.left-width)/2),y=work.top+std::max(0,int(work.bottom-work.top-height)/2);
        const auto window=CreateWindowExW(0,type.lpszClassName,L"ShardBound Editor",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            x,y,width,height,nullptr,nullptr,type.hInstance,&editor);
        if(!window) return 1;
        if(blockbenchTest) {
            ShowWindow(window,SW_SHOWNOACTIVATE);
            PostMessageW(window,WM_APP+3,0,0);
        } else if(smoke) PostMessageW(window,WM_APP+1,0,0);else ShowWindow(window,SW_SHOW);
        ACCEL shortcuts[]{{FVIRTKEY|FCONTROL,'S',SaveMap},{FVIRTKEY|FCONTROL,'O',OpenMap},
            {FVIRTKEY|FCONTROL,'Z',Undo},{FVIRTKEY|FCONTROL,'Y',Redo},
            {FVIRTKEY|FCONTROL,'T',NewTab},{FVIRTKEY|FCONTROL,'W',CloseTab},
            {FVIRTKEY|FCONTROL,VK_TAB,CycleTabs},{FVIRTKEY|FCONTROL|FSHIFT,VK_TAB,ReverseTabs}};
        const auto keys=CreateAcceleratorTableW(shortcuts,int(std::size(shortcuts)));
        MSG message{};int result;
        while((result=GetMessageW(&message,nullptr,0,0))>0) {
            if(editor.blockbenchInput(message.hwnd) ||
                (!TranslateAcceleratorW(window,keys,&message) && !IsDialogMessageW(window,&message))) {
                TranslateMessage(&message);DispatchMessageW(&message);
            }
        }
        if(keys) DestroyAcceleratorTable(keys);
        return result<0?1:int(message.wParam);
    } catch(const std::exception& error) {
        const auto message=wide(error.what());MessageBoxW(nullptr,message.c_str(),L"ShardBound Editor",MB_ICONERROR);return 1;
    }
}
