#include "tools/BlockbenchPane.h"
#include "tools/EditorTheme.h"
#include <objbase.h>
#include <wrl.h>
#include <WebView2.h>
#include <stdexcept>
#include <string_view>

namespace game::editor {
namespace {
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
constexpr wchar_t address[]=L"https://web.blockbench.net/";
bool officialPage(const wchar_t* uri) {
    return uri && std::wstring_view(uri).starts_with(address);
}
}

struct BlockbenchPane::State : std::enable_shared_from_this<State> {
    HWND owner{},host{};
    std::filesystem::path profile;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> web;
    EventRegistrationToken navigation{},completed{},popup{},messageToken{};
    bool pending=false,visible=false;
    std::wstring message=L"Carregando o Blockbench…";
    std::wstring script;
    BlockEvent event;

    void tell(std::wstring text) {
        message=std::move(text);
        if(host) ShowWindow(host,visible && message.empty()?SW_SHOWNA:SW_HIDE);
        if(owner) InvalidateRect(owner,nullptr,FALSE);
    }
    void fail(HRESULT error) {
        pending=false;
        if(error==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            tell(L"O runtime Microsoft Edge WebView2 não foi encontrado. Instale-o e tente novamente.");
        else tell(L"Não foi possível iniciar o Blockbench integrado. Tente novamente.");
    }
    void resize() {
        if(!controller || !host) return;
        RECT bounds{};GetClientRect(host,&bounds);
        controller->put_Bounds(bounds);
        controller->NotifyParentWindowPositionChanged();
    }
    void connect(ICoreWebView2Controller* created) {
        pending=false;controller=created;
        controller->get_CoreWebView2(&web);
        if(!web) {fail(E_FAIL);return;}
        ComPtr<ICoreWebView2Settings> settings;
        if(SUCCEEDED(web->get_Settings(&settings))) {
            settings->put_AreHostObjectsAllowed(FALSE);
            settings->put_IsWebMessageEnabled(event?TRUE:FALSE);
            settings->put_AreDevToolsEnabled(FALSE);
        }
        web->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                PWSTR raw=nullptr;
                if(SUCCEEDED(args->get_Uri(&raw))) {
                    if(!officialPage(raw)) args->put_Cancel(TRUE);
                    CoTaskMemFree(raw);
                }
                return S_OK;
            }).Get(),&navigation);
        web->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);
                return S_OK;
            }).Get(),&popup);
        web->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [weak=weak_from_this()](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                if(const auto self=weak.lock();self && self->owner) {
                    BOOL success=FALSE;args->get_IsSuccess(&success);
                    if(success) {
                        self->tell(L"");
                        if(self->event) {
                            try {self->event(nlohmann::json{{"kind","loaded"}});}
                            catch(const std::exception&) {self->tell(L"Não foi possível abrir o projeto do bloco.");}
                        }
                    }
                    else {
                        COREWEBVIEW2_WEB_ERROR_STATUS error{};
                        args->get_WebErrorStatus(&error);
                        if(error!=COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED)
                            self->tell(L"Falha ao carregar o Blockbench. Verifique a conexão e tente novamente.");
                    }
                }
                return S_OK;
            }).Get(),&completed);
        resize();controller->put_IsVisible(visible);
        if(!event) {
            if(FAILED(web->Navigate(address))) fail(E_FAIL);
            return;
        }
        if(FAILED(web->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [weak=weak_from_this()](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                const auto self=weak.lock();
                if(!self || !self->owner) return S_OK;
                PWSTR source=nullptr;
                args->get_Source(&source);
                const std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> origin(source,CoTaskMemFree);
                if(!source || std::wstring_view(source)!=address) return S_OK;
                PWSTR raw=nullptr;
                if(FAILED(args->get_WebMessageAsJson(&raw)) || !raw) return S_OK;
                const std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> json(raw,CoTaskMemFree);
                const std::wstring_view value(raw);
                if(value.size()>66*1024*1024) return S_OK;
                try {
                    const auto data=nlohmann::json::parse(value);
                    if(data.is_object()) self->event(data);
                } catch(const std::exception&) {
                    return E_INVALIDARG;
                }
                return S_OK;
            }).Get(),&messageToken))) {fail(E_FAIL);return;}
        const auto added=web->AddScriptToExecuteOnDocumentCreated(script.c_str(),
            Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
                [weak=weak_from_this()](HRESULT error,LPCWSTR) -> HRESULT {
                    if(const auto self=weak.lock();self && self->owner) {
                        if(FAILED(error) || FAILED(self->web->Navigate(address))) self->fail(E_FAIL);
                    }
                    return S_OK;
                }).Get());
        if(FAILED(added)) fail(added);
    }
    void start() {
        if(pending || controller) return;
        pending=true;tell(L"Carregando o Blockbench…");
        const auto result=CreateCoreWebView2EnvironmentWithOptions(nullptr,profile.c_str(),nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [weak=weak_from_this()](HRESULT error,ICoreWebView2Environment* env) -> HRESULT {
                    const auto self=weak.lock();
                    if(!self || !self->host) return S_OK;
                    if(FAILED(error) || !env) {self->fail(error);return S_OK;}
                    const auto result=env->CreateCoreWebView2Controller(self->host,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [weak](HRESULT error,ICoreWebView2Controller* controller) -> HRESULT {
                                if(const auto self=weak.lock();self && self->host) {
                                    if(FAILED(error) || !controller) self->fail(error);
                                    else self->connect(controller);
                                } else if(controller) controller->Close();
                                return S_OK;
                            }).Get());
                    if(FAILED(result)) self->fail(result);
                    return S_OK;
                }).Get());
        if(FAILED(result)) fail(result);
    }
};

BlockbenchPane::BlockbenchPane(HWND owner,std::filesystem::path profile,std::wstring script,BlockEvent event)
    :state_(std::make_shared<State>()) {
    state_->owner=owner;state_->profile=std::move(profile);state_->script=std::move(script);state_->event=std::move(event);
    state_->host=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,0,0,
        owner,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!state_->host) throw std::runtime_error("Não foi possível abrir a aba do Blockbench");
}

BlockbenchPane::~BlockbenchPane() {
    auto& state=*state_;
    state.owner=nullptr;
    if(state.web) {
        state.web->remove_NavigationStarting(state.navigation);
        state.web->remove_NavigationCompleted(state.completed);
        state.web->remove_NewWindowRequested(state.popup);
        if(state.event) state.web->remove_WebMessageReceived(state.messageToken);
    }
    if(state.controller) state.controller->Close();
    if(IsWindow(state.host)) DestroyWindow(state.host);
    state.host=nullptr;
}

void BlockbenchPane::show(const RECT& bounds,bool visible) {
    auto& state=*state_;
    state.visible=visible;
    if(!visible) {
        if(state.controller) state.controller->put_IsVisible(FALSE);
        ShowWindow(state.host,SW_HIDE);
        return;
    }
    MoveWindow(state.host,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,TRUE);
    state.resize();
    if(state.controller) state.controller->put_IsVisible(TRUE);
    if(state.web && state.message.empty()) ShowWindow(state.host,SW_SHOWNA);
    else state.start();
}

void BlockbenchPane::retry() {
    if(state_->pending) return;
    if(state_->web) {
        state_->tell(L"Carregando o Blockbench…");
        state_->web->Navigate(address);
    } else state_->start();
}

void BlockbenchPane::execute(const std::wstring& code) {
    if(!state_->web || FAILED(state_->web->ExecuteScript(code.c_str(),nullptr)))
        throw std::runtime_error("Não foi possível acionar o Blockbench.");
}

bool BlockbenchPane::owns(HWND window) const {
    return window==state_->host || IsChild(state_->host,window);
}

std::wstring BlockbenchPane::status() const {
    return state_->message;
}
}
