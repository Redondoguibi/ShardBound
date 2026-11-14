#include "tools/AiDialog.h"
#include "tools/EditorTheme.h"
#include "tools/EditorProcess.h"
#include "tools/EditorWorkspace.h"
#include "core/AtomicFile.h"
#include "core/PathText.h"
#include "world/WorldDefinition.h"
#include <array>
#include <chrono>
#include <commdlg.h>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>

namespace game {
namespace {
using Json=nlohmann::json;
enum { Provider=101, SaveKey, Start, Cancel, Apply, ChooseBridge, KeyLabel, ConnectionState };
constexpr UINT update=WM_APP+18;

std::wstring wide(const std::string& value) {
    const int n=MultiByteToWideChar(CP_UTF8,0,value.data(),int(value.size()),nullptr,0);
    std::wstring result(size_t(n),L'\0');
    if(n) MultiByteToWideChar(CP_UTF8,0,value.data(),int(value.size()),result.data(),n);
    return result;
}
std::wstring read(HWND control) {
    const auto length=GetWindowTextLengthW(control);
    std::wstring text(size_t(length)+1,L'\0');GetWindowTextW(control,text.data(),length+1);text.resize(size_t(length));return text;
}
std::string utf8(const std::wstring& input) {
    const int n=WideCharToMultiByte(CP_UTF8,0,input.data(),int(input.size()),nullptr,0,nullptr,nullptr);
    std::string result(size_t(n),'\0');
    if(n) WideCharToMultiByte(CP_UTF8,0,input.data(),int(input.size()),result.data(),n,nullptr,nullptr);
    return result;
}

class AiDialog {
public:
    AiDialog(const FiniteMap& map,const WorldDefinition& catalog,std::filesystem::path mods,std::filesystem::path program,std::filesystem::path bridge)
        : baseMap_(map),baseCatalog_(catalog),map_(map),catalog_(catalog),mods_(std::move(mods)),program_(std::move(program)),bridge_(std::move(bridge)) {
        const auto candidate=program_.parent_path().parent_path()/"BLOCKBENCH/bridge/mcp/server.mjs";
        if(bridge_.empty() && std::filesystem::is_regular_file(candidate)) bridge_=candidate;
    }
    AiDraft run(HWND parent) {
        WNDCLASSW type{};type.lpfnWndProc=dispatch;type.hInstance=GetModuleHandleW(nullptr);
        type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        type.lpszClassName=L"ShardBoundAiDialog";
        if(!RegisterClassW(&type) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register AI panel");
        const auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"Assistente do ShardBound",WS_CAPTION|WS_SYSMENU|WS_POPUP,
            CW_USEDEFAULT,CW_USEDEFAULT,838,704,parent,nullptr,type.hInstance,this);
        if(!window) throw std::runtime_error("Cannot open AI panel");
        editor::darkCaption(window);editor::centerDialog(window,parent);
        EnableWindow(parent,FALSE);ShowWindow(window,SW_SHOW);SetFocus(prompt_);
        MSG message{};BOOL result=TRUE;
        while(!closed_ && (result=GetMessageW(&message,nullptr,0,0))>0) {
            if(!IsDialogMessageW(window,&message)) {TranslateMessage(&message);DispatchMessageW(&message);}
        }
        worker_.request_stop();condition_.notify_all();if(worker_.joinable()) worker_.join();
        EnableWindow(parent,TRUE);DestroyWindow(window);SetActiveWindow(parent);
        if(result==0) PostQuitMessage(int(message.wParam));
        return accepted_?AiDraft{std::move(revisions_)}:AiDraft{};
    }

private:
    const FiniteMap& baseMap_;
    const WorldDefinition& baseCatalog_;
    FiniteMap map_;
    WorldDefinition catalog_;
    std::filesystem::path mods_,program_,work_,currentContent_,bridge_;
    HWND window_{},provider_{},model_{},key_{},mode_{},prompt_{},log_{};
    std::mutex mutex_;
    std::condition_variable_any condition_;
    std::vector<std::string> logs_;
    std::optional<std::wstring> review_;
    std::optional<bool> decision_;
    std::vector<AiMapRevision> revisions_;
    size_t revisionBytes_=0;
    size_t contentRevision_=0;
    bool terminalRan_=false;
    bool finished_=false,succeeded_=false,accepted_=false,closed_=false;
    std::jthread worker_;

    HWND child(const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0) {
        const auto control=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|editor::controlStyle(type,style),x,y,w,h,window_,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(!control) throw std::runtime_error("Cannot create AI panel control");
        editor::styleControl(control);
        return control;
    }
    void controls() {
        auto title=child(L"STATIC",L"Assistente IA",0,22,13,466,37);
        SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(editor::font(25,FW_SEMIBOLD)),FALSE);
        child(L"STATIC",L"Atividade",0,22,63,515,24);
        child(L"STATIC",L"Conexão",0,574,63,228,24);
        provider_=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,574,94,228,180,Provider);
        for(const auto option:{L"Codex local",L"OpenRouter",L"OpenAI",L"Anthropic",L"Google Gemini"})
            SendMessageW(provider_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(option));
        SendMessageW(provider_,CB_SETCURSEL,0,0);
        child(L"STATIC",L"Modelo (opcional no Codex)",0,574,134,228,22);
        model_=child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,574,161,228,28);
        child(L"STATIC",L"Chave de API",0,574,200,228,22,KeyLabel);
        key_=child(L"EDIT",L"",WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL,574,227,228,28);
        child(L"BUTTON",L"Salvar chave",WS_TABSTOP,574,264,228,29,SaveKey);
        child(L"STATIC",L"",0,574,306,228,51,ConnectionState);
        child(L"STATIC",L"Execução de ações",0,574,380,228,22);
        mode_=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,574,407,228,100);
        for(const auto option:{L"Revisar ações sensíveis",L"Modo autônomo"})
            SendMessageW(mode_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(option));
        SendMessageW(mode_,CB_SETCURSEL,0,0);
        child(L"BUTTON",L"Ponte do Blockbench…",WS_TABSTOP,574,456,228,31,ChooseBridge);
        child(L"STATIC",L"O que o assistente deve fazer?",0,22,464,534,23);
        prompt_=child(L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,22,492,524,107);
        log_=child(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,22,94,524,356);
        child(L"BUTTON",L"Executar tarefa",WS_TABSTOP,22,610,136,34,Start);
        child(L"BUTTON",L"Cancelar / fechar",WS_TABSTOP,166,610,142,34,Cancel);
        child(L"BUTTON",L"Aplicar rascunho",WS_TABSTOP,636,610,166,34,Apply);
        EnableWindow(GetDlgItem(window_,Apply),FALSE);selectProvider();
    }
    AiProvider provider() const {return static_cast<AiProvider>(SendMessageW(provider_,CB_GETCURSEL,0,0));}
    void chooseBridge() {
        std::array<wchar_t,32768> file{};
        OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
        dialog.lpstrFilter=L"Bridge do Blockbench\0server.mjs\0";dialog.lpstrFile=file.data();dialog.nMaxFile=DWORD(file.size());
        dialog.Flags=OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(GetOpenFileNameW(&dialog)) {bridge_=file.data();append(L"Bridge selecionado: "+bridge_.wstring()+L"\r\n");}
    }
    void selectProvider() {
        const bool api=provider()!=AiProvider::Codex;
        ShowWindow(key_,api?SW_SHOWNA:SW_HIDE);
        ShowWindow(GetDlgItem(window_,KeyLabel),api?SW_SHOWNA:SW_HIDE);
        ShowWindow(GetDlgItem(window_,SaveKey),api?SW_SHOWNA:SW_HIDE);
        EnableWindow(key_,api);EnableWindow(GetDlgItem(window_,SaveKey),api);
        SetWindowTextW(key_,L"");
        SetWindowTextW(GetDlgItem(window_,ConnectionState),api?(editorAiHasKey(provider())?L"Chave salva para este fornecedor.":L"Salve a chave e informe o ID do modelo."):
            L"Usa o login da instalação local do Codex.");
    }
    void append(const std::wstring& message) {
        SendMessageW(log_,EM_SETSEL,WPARAM(-1),LPARAM(-1));SendMessageW(log_,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(message.c_str()));
        SendMessageW(log_,EM_SCROLLCARET,0,0);
    }
    static int integer(const Json& value,int low,int high) {
        if(!value.is_number_integer()) throw std::runtime_error("Expected integer");
        const auto n=value.get<double>();if(n<low || n>high) throw std::runtime_error("Coordinate out of range");return int(n);
    }
    void refreshCatalog() {
        auto discovery=discoverMods(work_/"content");
        for(const auto& diagnostic:discovery.diagnostics) if(diagnostic.code!="MOD106") throw DiagnosticError(diagnostic);
        catalog_=WorldDefinition::load(discovery.mods);
    }
    void captureRevision(std::string label) {
        const auto b=map_.bounds();
        const size_t cost=size_t(b.width)*b.depth*FiniteMap::height*sizeof(std::uint16_t);
        if(revisions_.size()==64 || revisionBytes_+cost>96*1024*1024) throw std::runtime_error("Limite de revisões de mapa por tarefa");
        revisions_.push_back({map_,label.substr(0,120),currentContent_});revisionBytes_+=cost;
    }
    Json tool(const Json& action,std::stop_token stop) {
        const auto name=action.at("tool").get<std::string>();
        const auto b=map_.bounds();
        if(name=="read_map") {
            Json result={{"bounds",{b.minX,b.minZ,b.width,b.depth}},{"spawn",{map_.spawn().x,map_.spawn().z}},
                {"interiors",map_.interiors().size()},{"props",map_.props().size()}};
            if(action.contains("x") && action.contains("z")) {
                const int x=integer(action.at("x"),-1000000,1000000),z=integer(action.at("z"),-1000000,1000000);
                auto column=Json::array();for(int y=0;y<FiniteMap::height;++y) column.push_back(map_.block(x,y,z));
                result["column"]=std::move(column);
            }
            return result;
        }
        if(name=="read_catalog") {
            Json blocks=Json::array(),structures=Json::array();const auto query=action.value("query",std::string{});
            for(const auto& [id,block]:catalog_.blocks) if(id.find(query)!=std::string::npos && blocks.size()<512) blocks.push_back({id,block.solid});
            for(const auto& [id,structure]:catalog_.structures) if(id.find(query)!=std::string::npos) structures.push_back({id,structure.cells.size()});
            return {{"blocks",blocks},{"structures",structures}};
        }
        if(name=="test_map") {
            map_.validateDoors(catalog_);
            map_.save(work_/"preview.json");
            const auto result=runEditorProcess((program_/"game.exe").wstring(),{L"--hidden",L"--no-vsync",L"--frames",L"30",
                L"--test-map",(work_/"preview.json").wstring(),L"--mods",(work_/"content").wstring(),
                L"--user-dir",(work_/"preview-user").wstring()},work_,{},stop);
            return {{"exitCode",result.exitCode},{"output",result.output.substr(0,12000)}};
        }
        if(name=="reload_content") {
            refreshCatalog();
            (void)FiniteMap::parse(map_.json(),catalog_);
            return {{"blocks",catalog_.blocks.size()},{"structures",catalog_.structures.size()}};
        }
        if(name=="capture_terminal") {
            terminalRan_=true;
            if(contentRevision_>=16 || revisions_.size()>=64) throw std::runtime_error("Limite de revisões de terminal");
            const auto snapshot=work_/"revisions"/std::to_string(++contentRevision_);
            std::filesystem::create_directories(snapshot.parent_path());
            std::filesystem::copy(work_/"content",snapshot,std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks);
            refreshCatalog();
            const auto preview=work_/"preview.json";
            if(action.at("reload_map").get<bool>()) map_=FiniteMap::read(preview,catalog_);
            else (void)FiniteMap::parse(map_.json(),catalog_);
            currentContent_=snapshot;
            captureRevision("Terminal · "+action.at("label").get<std::string>());
            return {{"revision",contentRevision_}};
        }
        if(name=="blockbench") {
            if(bridge_.empty() || !std::filesystem::is_regular_file(bridge_))
                throw std::runtime_error("Selecione o arquivo server.mjs em Bridge do Blockbench antes de executar a tarefa");
            atomicWrite(work_/"blockbench.json",Json{{"name",action.at("name")},{"args",action.value("args",Json::object())}}.dump());
            const auto result=runEditorProcess(L"node.exe",{(program_/"editor-studio.mjs").wstring(),bridge_.wstring(),L"tool",
                (work_/"blockbench.json").wstring()},work_,{},stop);
            if(result.exitCode) throw std::runtime_error(result.output.substr(0,1800));
            return Json::parse(result.output);
        }
        const size_t revisionCost=size_t(b.width)*b.depth*FiniteMap::height*sizeof(std::uint16_t);
        if(revisions_.size()==64 || revisionBytes_+revisionCost>96*1024*1024) throw std::runtime_error("Limite de revisões de mapa por tarefa");
        if(name=="set_spawn") map_.setSpawn(integer(action.at("x"),b.minX,b.minX+b.width-1),integer(action.at("z"),b.minZ,b.minZ+b.depth-1));
        else if(name=="set_block") map_.setBlock(integer(action.at("x"),b.minX,b.minX+b.width-1),integer(action.at("y"),0,47),
            integer(action.at("z"),b.minZ,b.minZ+b.depth-1),action.at("block").get<std::string>(),catalog_);
        else if(name=="fill") {
            const auto& a=action.at("from");const auto& c=action.at("to");
            if(!a.is_array() || !c.is_array() || a.size()!=3 || c.size()!=3) throw std::runtime_error("Expected fill corners");
            const int x=integer(a[0],b.minX,b.minX+b.width-1),y=integer(a[1],0,47),z=integer(a[2],b.minZ,b.minZ+b.depth-1);
            const int x2=integer(c[0],x,b.minX+b.width-1),y2=integer(c[1],y,47),z2=integer(c[2],z,b.minZ+b.depth-1);
            if((x2-x+1)*(y2-y+1)*(z2-z+1)>262144) throw std::runtime_error("Fill exceeds 262144 cells");
            const auto block=action.at("block").get<std::string>();
            for(int yy=y;yy<=y2;++yy) for(int zz=z;zz<=z2;++zz) for(int xx=x;xx<=x2;++xx) map_.setBlock(xx,yy,zz,block,catalog_);
        } else if(name=="place_structure") map_.placeStructures(Json::array({{action.at("id"),action.at("x"),action.at("y"),action.at("z"),action.at("rotation")}}),catalog_);
        else if(name=="place_model") map_.addProp({action.at("id").get<std::string>(),integer(action.at("x"),b.minX,b.minX+b.width-1),
            integer(action.at("y"),0,47),integer(action.at("z"),b.minZ,b.minZ+b.depth-1),action.value("rotation",0.f),action.value("scale",1.f)});
        else if(name=="link_interior") {
            const auto point=[&](const Json& value) {
                if(!value.is_array() || value.size()!=2) throw std::runtime_error("Expected point [x,z]");
                return FiniteMap::Spawn{integer(value[0],b.minX,b.minX+b.width-1),integer(value[1],b.minZ,b.minZ+b.depth-1)};
            };
            auto inside=std::make_shared<FiniteMap>(FiniteMap::parse(action.at("map"),catalog_));
            if(!inside->exit() || !inside->interiors().empty()) throw std::runtime_error("Interior needs an exit and cannot contain another room");
            auto next=map_;
            next.addInterior({point(action.at("entrance")),point(action.at("return")),*inside->exit(),inside});
            next.validateDoors(catalog_);map_=std::move(next);
        }
        else if(name=="read_draft") {
            auto staged=FiniteMap::read(work_/"preview.json",catalog_);staged.validateDoors(catalog_);map_=std::move(staged);
        } else throw std::runtime_error("Unknown editor tool");
        map_.save(work_/"preview.json");
        captureRevision(action.value("label",name));return {{"ok",true}};
    }
    void start() {
        if(worker_.joinable()) worker_.join();
        EditorAiRequest request;
        request.provider=provider();request.model=utf8(read(model_));request.prompt=utf8(read(prompt_));
        request.autonomous=SendMessageW(mode_,CB_GETCURSEL,0,0)==1;request.programDirectory=program_;
        if(request.prompt.empty()) throw std::runtime_error("Descreva a tarefa");
        if(request.provider!=AiProvider::Codex && !editorAiHasKey(request.provider)) throw std::runtime_error("Salve a chave do fornecedor");
        if(request.autonomous && MessageBoxW(window_,L"A tarefa poderá executar comandos do PowerShell com as permissões do seu usuário. Iniciar?",
            L"Modo autônomo",MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
        work_=createEditorRevision("ai");
        map_=baseMap_;catalog_=baseCatalog_;revisions_.clear();revisionBytes_=0;contentRevision_=0;terminalRan_=false;currentContent_=mods_;
        SetWindowTextW(log_,L"");
        EnableWindow(GetDlgItem(window_,Start),FALSE);EnableWindow(GetDlgItem(window_,Apply),FALSE);EnableWindow(provider_,FALSE);
        EnableWindow(key_,FALSE);EnableWindow(GetDlgItem(window_,SaveKey),FALSE);
        EnableWindow(GetDlgItem(window_,ChooseBridge),FALSE);
        request.work=work_;finished_=false;succeeded_=false;
        worker_=std::jthread([this,request=std::move(request)](std::stop_token stop) {
            const auto status=[&](const std::string& text) {
                {std::lock_guard lock(mutex_);logs_.push_back(text);}PostMessageW(window_,update,0,0);
                std::ofstream audit(work_/"activity.jsonl",std::ios::binary|std::ios::app);
                if(audit) audit<<Json{{"message",text}}.dump(-1,' ',false,Json::error_handler_t::replace)<<'\n';
            };
            const auto approval=[&](const std::wstring& text) {
                std::unique_lock lock(mutex_);decision_.reset();review_=text;PostMessageW(window_,update,0,0);
                return condition_.wait(lock,stop,[&]{return decision_.has_value();}) && *decision_;
            };
            try {
                std::filesystem::create_directories(work_);
                map_.save(work_/"preview.json");
                std::filesystem::copy(mods_,work_/"content",std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_symlinks);
                status("Diretório da tarefa: "+pathText(work_));
                const auto message=runEditorAi(request,[&](const Json& action){return tool(action,stop);},approval,status,stop);
                refreshCatalog();
                (void)FiniteMap::parse(map_.json(),catalog_);
                const auto checked=runEditorProcess((program_/"modcheck.exe").wstring(),{(work_/"content").wstring(),L"--json"},work_,{},stop);
                if(checked.exitCode) throw std::runtime_error("Validação do rascunho falhou:\n"+checked.output.substr(0,10000));
                if(terminalRan_ && (revisions_.empty() || currentContent_!=work_/"revisions"/std::to_string(contentRevision_))) {
                    currentContent_=work_/"content";
                    captureRevision("Arquivos validados");
                } else if(terminalRan_) revisions_.back().content=work_/"content";
                {std::lock_guard lock(mutex_);succeeded_=true;}
                status(message.empty()?"Tarefa concluída. Revise o mapa antes de aplicar.":message);
            } catch(const std::exception& error) {status(error.what());}
            {std::lock_guard lock(mutex_);finished_=true;}PostMessageW(window_,update,0,0);
        });
    }
    void deliver() {
        std::vector<std::string> logs;
        std::optional<std::wstring> review;
        bool finished=false,succeeded=false;
        {std::lock_guard lock(mutex_);logs.swap(logs_);review.swap(review_);finished=finished_;succeeded=succeeded_;}
        for(const auto& entry:logs) append(wide(entry)+L"\r\n\r\n");
        if(review) {
            const bool allow=MessageBoxW(window_,review->c_str(),L"Revisar ação da IA",MB_YESNO|MB_ICONQUESTION)==IDYES;
            {std::lock_guard lock(mutex_);decision_=allow;}condition_.notify_all();
        }
        if(finished) {
            EnableWindow(GetDlgItem(window_,Start),TRUE);EnableWindow(provider_,TRUE);
            EnableWindow(GetDlgItem(window_,ChooseBridge),TRUE);
            EnableWindow(key_,provider()!=AiProvider::Codex);EnableWindow(GetDlgItem(window_,SaveKey),provider()!=AiProvider::Codex);
            EnableWindow(GetDlgItem(window_,Apply),succeeded);
        }
    }
    void command(int id,int code) {
        if(id==Provider && code==CBN_SELCHANGE) selectProvider();
        if(id==SaveKey) {editorAiSaveKey(provider(),read(key_));SetWindowTextW(key_,L"");append(L"Chave salva no Windows.\r\n");}
        if(id==Start) start();
        if(id==ChooseBridge) chooseBridge();
        if(id==Cancel) close();
        if(id==Apply) {accepted_=true;closed_=true;}
    }
    void close() {
        bool finished;
        {std::lock_guard lock(mutex_);finished=finished_;}
        if(worker_.joinable() && !finished) {worker_.request_stop();condition_.notify_all();append(L"Cancelando…\r\n");return;}
        closed_=true;
    }
    static LRESULT CALLBACK dispatch(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        if(message==WM_NCCREATE) {
            const auto create=reinterpret_cast<const CREATESTRUCTW*>(lParam);
            const auto self=static_cast<AiDialog*>(create->lpCreateParams);self->window_=window;
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        const auto self=reinterpret_cast<AiDialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(!self) return DefWindowProcW(window,message,wParam,lParam);
        LRESULT themed=0;
        if(editor::paintDialog(window,message,wParam,lParam,themed,Apply)) return themed;
        try {
            if(message==WM_CREATE) {self->controls();return 0;}
            if(message==WM_COMMAND) {self->command(LOWORD(wParam),HIWORD(wParam));return 0;}
            if(message==WM_CLOSE) {self->close();return 0;}
            if(message==update) {self->deliver();return 0;}
        } catch(const std::exception& error) {MessageBoxW(window,wide(error.what()).c_str(),L"Assistente",MB_ICONERROR);return message==WM_CREATE?-1:0;}
        return DefWindowProcW(window,message,wParam,lParam);
    }
};
}

AiDraft authorMapWithAi(HWND parent,const FiniteMap& map,const WorldDefinition& catalog,
    const std::filesystem::path& mods,const std::filesystem::path& programDirectory,const std::filesystem::path& bridge) {
    return AiDialog(map,catalog,mods,programDirectory,bridge).run(parent);
}
}
