#include "tools/EquipmentDialog.h"
#include "tools/EditorTheme.h"
#include "tools/EditorProcess.h"
#include "core/AtomicFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <commdlg.h>
#include <stdexcept>

namespace game {
namespace {
enum { Kind=101,AddGround,AddAir,Remove,Up,Down,Steps,ChooseStudio,Publish,Cancel,SyncClips,ExportStudio,PreviewAttack,PreviewShape,OpenClip,OpenIdle,
    GeneralTab, CombatTab };

std::string utf8(const std::wstring& input) {
    const int n=WideCharToMultiByte(CP_UTF8,0,input.data(),int(input.size()),nullptr,0,nullptr,nullptr);
    std::string result(size_t(n),'\0');
    if(n) WideCharToMultiByte(CP_UTF8,0,input.data(),int(input.size()),result.data(),n,nullptr,nullptr);
    return result;
}
std::wstring wide(const std::string& input) {
    const int n=MultiByteToWideChar(CP_UTF8,0,input.data(),int(input.size()),nullptr,0);
    std::wstring result(size_t(n),L'\0');
    if(n) MultiByteToWideChar(CP_UTF8,0,input.data(),int(input.size()),result.data(),n);
    return result;
}

class EquipmentDialog {
public:
    EquipmentDialog(std::filesystem::path program,std::filesystem::path bridge):program_(std::move(program)),bridge_(std::move(bridge)) {}
    std::optional<AuthoredEquipment> run(HWND parent) {
        WNDCLASSW type{};type.lpfnWndProc=dispatch;type.hInstance=GetModuleHandleW(nullptr);
        type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        type.lpszClassName=L"ShardBoundEquipmentDialog";
        if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register equipment editor");
        const auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"Novo equipamento",WS_CAPTION|WS_SYSMENU|WS_POPUP,
            CW_USEDEFAULT,CW_USEDEFAULT,744,667,parent,nullptr,type.hInstance,this);
        if(!window) throw std::runtime_error("Cannot open equipment editor");
        editor::darkCaption(window);editor::centerDialog(window,parent);
        EnableWindow(parent,FALSE);ShowWindow(window,SW_SHOW);SetFocus(mod_);
        MSG message{};BOOL result=TRUE;
        while(!done_ && (result=GetMessageW(&message,nullptr,0,0))>0) {
            if(!IsDialogMessageW(window,&message)) {TranslateMessage(&message);DispatchMessageW(&message);}
        }
        EnableWindow(parent,TRUE);DestroyWindow(window);SetActiveWindow(parent);
        if(result==0) PostQuitMessage(int(message.wParam));
        return accepted_?std::optional{std::move(equipment_)}:std::nullopt;
    }

private:
    struct Step { bool air;AuthoredAttack attack; };
    HWND window_{},mod_{},name_{},model_{},kind_{},scale_{},protection_{},movementBonus_{},healthBonus_{},grant_{},steps_{},clip_{},shape_{};
    std::array<HWND,6> values_{};
    std::vector<Step> stepsData_{{false,{"ground_1","ground_1"}}};
    std::optional<size_t> selected_{0};
    unsigned groundId_=1,airId_=0;
    AuthoredEquipment equipment_;
    std::filesystem::path program_,bridge_;
    bool done_=false,accepted_=false;
    bool combat_=false;
    std::vector<HWND> generalControls_,combatControls_;
    std::vector<HWND>* currentSection_=nullptr;

    HWND child(const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0) {
        const auto control=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|editor::controlStyle(type,style),x,y,w,h,window_,reinterpret_cast<HMENU>(INT_PTR(id)),
            GetModuleHandleW(nullptr),nullptr);
        if(!control) throw std::runtime_error("Cannot create equipment editor control");
        editor::styleControl(control);
        if(currentSection_) currentSection_->push_back(control);
        return control;
    }
    HWND edit(const wchar_t* label,const wchar_t* value,int x,int y,int w=190) {
        child(L"STATIC",label,0,x,y+3,130,22);
        return child(L"EDIT",value,WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,x+131,y,w,24);
    }
    HWND combo(int x,int y,int w,int id=0) {return child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,x,y,w,160,id);}
    void add(HWND control,const wchar_t* label) {SendMessageW(control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));}
    static std::wstring get(HWND control) {
        const auto length=GetWindowTextLengthW(control);
        std::wstring text(size_t(length)+1,L'\0');GetWindowTextW(control,text.data(),length+1);text.resize(size_t(length));return text;
    }
    static double number(HWND control) {
        auto text=get(control);for(auto& c:text) if(c==L',') c=L'.';
        size_t end=0;const double value=std::stod(text,&end);
        if(end!=text.size()) throw std::runtime_error("Use números nos campos de atributos");
        return value;
    }
    void createControls() {
        auto title=child(L"STATIC",L"Criar equipamento",0,24,12,660,40);
        SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(editor::font(25,FW_SEMIBOLD)),FALSE);
        child(L"BUTTON",L"Dados e modelo",WS_TABSTOP,24,61,150,33,GeneralTab);
        child(L"BUTTON",L"Golpes e animação",WS_TABSTOP,182,61,162,33,CombatTab);
        currentSection_=&generalControls_;
        mod_=edit(L"ID do mod",L"",24,127,255);
        name_=edit(L"Nome no inventário",L"",24,171,255);
        child(L"STATIC",L"Tipo / slot",0,24,222,130,24);
        kind_=combo(155,216,255,Kind);
        model_=edit(L"ID do modelo",L"foundation:sword",24,265,255);
        child(L"BUTTON",L"Importar Studio…",WS_TABSTOP,432,265,136,31,ChooseStudio);
        child(L"BUTTON",L"Exportar do Blockbench",WS_TABSTOP,432,308,224,31,ExportStudio);
        for(const auto name:{L"Arma",L"Armadura — Cabeça",L"Armadura — Torso",L"Armadura — Pernas",L"Armadura — Pés"}) add(kind_,name);
        SendMessageW(kind_,CB_SETCURSEL,0,0);
        scale_=edit(L"Escala do modelo",L"0.05",24,308,125);
        protection_=edit(L"Proteção",L"4",24,412,125);
        movementBonus_=edit(L"Bônus de velocidade",L"0",24,453,125);
        healthBonus_=edit(L"Bônus de vida",L"0",24,494,125);
        grant_=child(L"BUTTON",L"Disponível no inventário ao começar",WS_TABSTOP|BS_AUTOCHECKBOX,24,355,325,26);
        SendMessageW(grant_,BM_SETCHECK,BST_CHECKED,0);
        currentSection_=&combatControls_;
        child(L"STATIC",L"Sequência de golpes",0,24,116,237,25);
        child(L"STATIC",L"Atributos do golpe selecionado",0,288,116,412,25);
        steps_=child(L"LISTBOX",L"",LBS_NOTIFY|WS_TABSTOP|WS_VSCROLL,24,149,237,285,Steps);
        child(L"BUTTON",L"+ Chão",WS_TABSTOP,16,474,71,28,AddGround);
        child(L"BUTTON",L"+ Aéreo",WS_TABSTOP,91,474,71,28,AddAir);
        child(L"BUTTON",L"Remover",WS_TABSTOP,166,474,87,28,Remove);
        child(L"BUTTON",L"Subir",WS_TABSTOP,16,508,71,24,Up);
        child(L"BUTTON",L"Descer",WS_TABSTOP,91,508,71,24,Down);
        const std::array labels{L"Dano",L"Alcance",L"Largura",L"Tempo até acerto (s)",L"Recuperação (s)",L"Altura do volume"};
        for(size_t i=0;i<labels.size();++i) values_[i]=edit(labels[i],L"",288,149+34*int(i),234);
        clip_=edit(L"Clipe no Blockbench",L"",288,359,234);
        child(L"STATIC",L"Área de acerto",0,288,402,130,24);
        shape_=combo(419,396,234,PreviewShape);
        for(const auto name:{L"Arco frontal",L"Círculo",L"Retângulo frontal"}) add(shape_,name);
        child(L"BUTTON",L"Criar clipes no Blockbench",WS_TABSTOP,405,451,207,27,SyncClips);
        child(L"BUTTON",L"Ver área de acerto",WS_TABSTOP,274,451,125,27,PreviewAttack);
        child(L"BUTTON",L"Abrir golpe",WS_TABSTOP,274,487,125,26,OpenClip);
        child(L"BUTTON",L"Abrir idle",WS_TABSTOP,405,487,99,26,OpenIdle);
        currentSection_=nullptr;
        child(L"BUTTON",L"Publicar equipamento",WS_TABSTOP,515,572,182,35,Publish);
        child(L"BUTTON",L"Cancelar",WS_TABSTOP,399,572,105,35,Cancel);
        updateSteps();updateKind();
    }
    bool armor() const {return SendMessageW(kind_,CB_GETCURSEL,0,0)>0;}
    void updateKind() {
        const bool attacks=!armor();
        if(!attacks) combat_=false;
        for(const auto control:generalControls_) ShowWindow(control,combat_?SW_HIDE:SW_SHOWNA);
        for(const auto control:combatControls_) ShowWindow(control,combat_?SW_SHOWNA:SW_HIDE);
        EnableWindow(GetDlgItem(window_,CombatTab),attacks);
        EnableWindow(protection_,!attacks);
        EnableWindow(movementBonus_,!attacks);EnableWindow(healthBonus_,!attacks);
        for(const int id:{AddGround,AddAir,Remove,Up,Down,Steps}) EnableWindow(GetDlgItem(window_,id),attacks);
        EnableWindow(GetDlgItem(window_,SyncClips),attacks);
        EnableWindow(GetDlgItem(window_,OpenClip),attacks && selected_.has_value());
        EnableWindow(GetDlgItem(window_,OpenIdle),attacks);
        EnableWindow(GetDlgItem(window_,PreviewAttack),attacks && selected_.has_value());
        for(const auto field:values_) EnableWindow(field,attacks && selected_.has_value());
        EnableWindow(shape_,attacks && selected_.has_value());EnableWindow(clip_,attacks && selected_.has_value());
        EnableWindow(values_[2],attacks && selected_.has_value() && SendMessageW(shape_,CB_GETCURSEL,0,0)!=1);
        for(auto field:{protection_,movementBonus_,healthBonus_}) {
            ShowWindow(field,!attacks && !combat_?SW_SHOWNA:SW_HIDE);
            ShowWindow(GetWindow(field,GW_HWNDPREV),!attacks && !combat_?SW_SHOWNA:SW_HIDE);
        }
        InvalidateRect(GetDlgItem(window_,GeneralTab),nullptr,FALSE);
        InvalidateRect(GetDlgItem(window_,CombatTab),nullptr,FALSE);
    }
    void remember() {
        if(!selected_ || armor()) return;
        auto attack=stepsData_.at(*selected_).attack;
        attack.damage=number(values_[0]);attack.reach=number(values_[1]);attack.width=number(values_[2]);
        attack.windup=number(values_[3]);attack.recovery=number(values_[4]);
        attack.height=number(values_[5]);
        attack.clip=utf8(get(clip_));
        constexpr std::array shapes{"arc","circle","box"};
        attack.shape=shapes.at(size_t(SendMessageW(shape_,CB_GETCURSEL,0,0)));
        stepsData_.at(*selected_).attack=std::move(attack);
    }
    void updateSteps() {
        SendMessageW(steps_,LB_RESETCONTENT,0,0);
        for(const auto& step:stepsData_) {
            const auto label=(step.air?L"Aéreo   ":L"Chão   ")+wide(step.attack.id);
            SendMessageW(steps_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        }
        if(selected_) {
            SendMessageW(steps_,LB_SETCURSEL,*selected_,0);
            const auto& a=stepsData_.at(*selected_).attack;
            const std::array values{a.damage,a.reach,a.width,a.windup,a.recovery,a.height};
            for(size_t i=0;i<values.size();++i) SetWindowTextW(values_[i],std::to_wstring(values[i]).c_str());
            SetWindowTextW(clip_,wide(a.clip).c_str());SendMessageW(shape_,CB_SETCURSEL,a.shape=="box"?2:a.shape=="circle"?1:0,0);
        }
        updateKind();
    }
    void selectStudio() {
        std::array<wchar_t,32768> file{};
        OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
        dialog.lpstrFilter=L"Manifesto do Studio\0*.json\0";dialog.lpstrFile=file.data();dialog.nMaxFile=DWORD(file.size());
        dialog.Flags=OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(GetOpenFileNameW(&dialog)) {
            equipment_.studioManifest=file.data();SetWindowTextW(model_,file.data());SetWindowTextW(scale_,L"1");
        }
    }
    nlohmann::json bridge(const std::wstring& operation,const nlohmann::json& plan={}) {
        if(bridge_.empty()) {
            auto candidate=program_.parent_path().parent_path()/"BLOCKBENCH/bridge/mcp/server.mjs";
            if(std::filesystem::is_regular_file(candidate)) bridge_=std::move(candidate);
        }
        if(bridge_.empty()) {
            std::array<wchar_t,32768> file{};
            OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
            dialog.lpstrFilter=L"Bridge do Blockbench\0server.mjs\0";dialog.lpstrFile=file.data();dialog.nMaxFile=DWORD(file.size());
            dialog.Flags=OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(!GetOpenFileNameW(&dialog)) return {};
            bridge_=file.data();
        }
        const auto work=std::filesystem::temp_directory_path()/"ShardBoundEditor"/
            ("studio-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        const auto planFile=work/"clips.json";
        atomicWrite(planFile,plan.dump());
        const auto result=runEditorProcess(L"node.exe",{(program_/"editor-studio.mjs").wstring(),bridge_.wstring(),operation,planFile.wstring()},work,{},{});
        if(result.exitCode) throw std::runtime_error(result.output.substr(0,1800));
        return nlohmann::json::parse(result.output);
    }
    void publish() {
        remember();equipment_.mod=utf8(get(mod_));equipment_.name=utf8(get(name_));equipment_.model=utf8(get(model_));
        if(get(model_)!=equipment_.studioManifest.wstring()) equipment_.studioManifest.clear();
        equipment_.scale=number(scale_);equipment_.grantOnStart=SendMessageW(grant_,BM_GETCHECK,0,0)==BST_CHECKED;
        if(armor()) {
            constexpr std::array slots{"","head","chest","legs","feet"};
            equipment_.armorSlot=slots.at(size_t(SendMessageW(kind_,CB_GETCURSEL,0,0)));equipment_.protection=number(protection_);
            equipment_.movementBonus=number(movementBonus_);equipment_.healthBonus=number(healthBonus_);
        } else for(const auto& step:stepsData_) (step.air?equipment_.air:equipment_.ground).push_back(step.attack);
        accepted_=done_=true;
    }
    void previewAttack() {
        remember();
        const auto& attack=stepsData_.at(*selected_).attack;
        if(!std::isfinite(attack.reach) || !std::isfinite(attack.width) || attack.reach<=0 || attack.width<=0)
            throw std::runtime_error("O alcance e a largura precisam ser positivos");
        WNDCLASSW type{};type.lpfnWndProc=[](HWND window,UINT message,WPARAM w,LPARAM l)->LRESULT {
            if(message==WM_NCCREATE) {
                const auto create=reinterpret_cast<const CREATESTRUCTW*>(l);
                SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            }
            if(message==WM_PAINT) {
                const auto shape=reinterpret_cast<const AuthoredAttack*>(GetWindowLongPtrW(window,GWLP_USERDATA));
                PAINTSTRUCT paint{};const auto dc=BeginPaint(window,&paint);
                if(shape) {
                    const double size=std::max({1.0,shape->reach,shape->shape=="circle"?0.0:shape->width*.5});
                    const auto radius=int(105*std::clamp(shape->reach/size,0.0,1.0));
                    const auto half=int(105*std::clamp(shape->width*.5/size,0.0,1.0));
                    const auto brush=CreateSolidBrush(RGB(226,154,77));const auto old=SelectObject(dc,brush);
                    if(shape->shape=="box") Rectangle(dc,150-half,148-radius,151+half,149);
                    else if(shape->shape=="circle") Ellipse(dc,150-radius,148-radius,151+radius,149+radius);
                    else {
                        std::array<POINT,65> outline{};
                        const auto limit=std::acos(-0.1);
                        for(size_t i=0;i<outline.size();++i) {
                            const auto angle=limit*(2.0*double(i)/double(outline.size()-1)-1.0);
                            const auto x=std::sin(angle),z=std::cos(angle);
                            const auto distance=std::min(double(radius),half/std::max(std::abs(x),.0001));
                            outline[i]={150+LONG(std::lround(distance*x)),148-LONG(std::lround(distance*z))};
                        }
                        Polygon(dc,outline.data(),int(outline.size()));
                    }
                    SelectObject(dc,old);DeleteObject(brush);
                    const auto player=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
                    const RECT dot{145,143,155,153};FillRect(dc,&dot,player);
                    const auto label=L"Alcance "+std::to_wstring(shape->reach)+L" m";
                    TextOutW(dc,16,270,label.c_str(),int(label.size()));
                }
                EndPaint(window,&paint);return 0;
            }
            return DefWindowProcW(window,message,w,l);
        };
        type.hInstance=GetModuleHandleW(nullptr);type.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);type.lpszClassName=L"ShardBoundAttackPreview";
        if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register attack preview");
        const auto window=CreateWindowExW(0,type.lpszClassName,L"Área de acerto vista de cima",WS_CAPTION|WS_SYSMENU|WS_POPUP,
            CW_USEDEFAULT,CW_USEDEFAULT,318,340,window_,nullptr,type.hInstance,const_cast<AuthoredAttack*>(&attack));
        if(!window) throw std::runtime_error("Cannot open attack preview");
        EnableWindow(window_,FALSE);ShowWindow(window,SW_SHOW);
        MSG message{};BOOL result=TRUE;
        while(IsWindow(window) && (result=GetMessageW(&message,nullptr,0,0))>0) {TranslateMessage(&message);DispatchMessageW(&message);}
        EnableWindow(window_,TRUE);SetActiveWindow(window_);
        if(result==0) PostQuitMessage(int(message.wParam));
    }
    void command(int id,int code) {
        if(id==GeneralTab || id==CombatTab) {
            if(combat_) remember();
            combat_=id==CombatTab && !armor();updateKind();return;
        }
        if(id==Kind) {updateKind();return;}
        if(id==PreviewShape && code==CBN_SELCHANGE) {updateKind();return;}
        if(id==Cancel) {done_=true;return;}
        if(id==ChooseStudio) {selectStudio();return;}
        if(id==SyncClips || id==OpenClip || id==OpenIdle) {
            remember();
            auto clips=nlohmann::json::array({{{"name","idle"},{"length",1},{"loop","loop"}}});
            for(const auto& step:stepsData_) {
                if(step.attack.clip.empty()) throw std::runtime_error("Informe o nome do clipe de cada golpe");
                clips.push_back({{"name",step.attack.clip},{"length",step.attack.windup+step.attack.recovery},{"loop","once"}});
            }
            nlohmann::json plan={{"namespace",utf8(get(mod_))},{"clips",clips}};
            if(id==OpenClip && selected_) plan["select"]=stepsData_.at(*selected_).attack.clip;
            if(id==OpenIdle) plan["select"]="idle";
            if(!bridge(L"clips",plan).is_null() && id==SyncClips)
                MessageBoxW(window_,L"Os clipes estão disponíveis na timeline do Blockbench.",L"Equipamento",MB_OK);
            return;
        }
        if(id==ExportStudio) {
            const auto result=bridge(L"export");
            if(!result.is_null()) {
                equipment_.studioManifest=wide(result.at("manifest").get<std::string>());
                SetWindowTextW(model_,equipment_.studioManifest.c_str());SetWindowTextW(scale_,L"1");
            }
            return;
        }
        if(id==Publish) {publish();return;}
        if(id==PreviewAttack && selected_) {previewAttack();return;}
        if(id==Steps && code==LBN_SELCHANGE) {
            const auto next=SendMessageW(steps_,LB_GETCURSEL,0,0);
            if(next!=LB_ERR) {remember();selected_=size_t(next);updateSteps();}return;
        }
        if(id==AddGround || id==AddAir) {
            remember();const bool air=id==AddAir;
            if(std::count_if(stepsData_.begin(),stepsData_.end(),[&](const Step& step){return step.air==air;})>=12)
                throw std::runtime_error("A sequência comporta até 12 golpes");
            const auto key=std::string(air?"air_":"ground_")+std::to_string(air?++airId_:++groundId_);
            const auto first=std::find_if(stepsData_.begin(),stepsData_.end(),[](const Step& step){return !step.air;});
            auto attack=first==stepsData_.end()?AuthoredAttack{}:first->attack;
            attack.id=key;attack.clip=key;
            stepsData_.push_back({air,std::move(attack)});selected_=stepsData_.size()-1;
        } else if(selected_) {
            const size_t current=*selected_;
            if(id==Remove) {
                stepsData_.erase(stepsData_.begin()+current);
                if(stepsData_.empty()) selected_.reset();else selected_=std::min(current,stepsData_.size()-1);
            } else if((id==Up && current>0) || (id==Down && current+1<stepsData_.size())) {
                remember();const size_t next=id==Up?current-1:current+1;
                std::swap(stepsData_[current],stepsData_[next]);selected_=next;
            } else return;
        }
        updateSteps();
    }
    static LRESULT CALLBACK dispatch(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        if(message==WM_NCCREATE) {
            const auto create=reinterpret_cast<const CREATESTRUCTW*>(lParam);
            const auto self=static_cast<EquipmentDialog*>(create->lpCreateParams);self->window_=window;
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        const auto self=reinterpret_cast<EquipmentDialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(!self) return DefWindowProcW(window,message,wParam,lParam);
        if(message==WM_DRAWITEM) {
            const auto& item=*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if(item.CtlID==GeneralTab || item.CtlID==CombatTab) {
                editor::drawButton(item,false,(item.CtlID==CombatTab)==self->combat_);return TRUE;
            }
        }
        LRESULT themed=0;
        if(editor::paintDialog(window,message,wParam,lParam,themed,Publish)) return themed;
        try {
            if(message==WM_CREATE) {self->createControls();return 0;}
            if(message==WM_COMMAND) {self->command(LOWORD(wParam),HIWORD(wParam));return 0;}
            if(message==WM_CLOSE) {self->done_=true;return 0;}
        } catch(const std::exception& error) {
            MessageBoxW(window,wide(error.what()).c_str(),L"Equipamento",MB_ICONERROR);return message==WM_CREATE?-1:0;
        }
        return DefWindowProcW(window,message,wParam,lParam);
    }
};
}
std::optional<AuthoredEquipment> editEquipment(HWND parent,const std::filesystem::path& programDirectory,const std::filesystem::path& bridge) {
    return EquipmentDialog{programDirectory,bridge}.run(parent);
}
}
