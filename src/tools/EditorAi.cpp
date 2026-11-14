#include "tools/EditorAi.h"
#include "tools/EditorProcess.h"
#include "core/AtomicFile.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <wincred.h>
#include <winhttp.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <algorithm>

namespace game {
namespace {
using Json=nlohmann::json;
using Http=std::unique_ptr<void,decltype(&WinHttpCloseHandle)>;
std::wstring wide(const std::string& value) {
    const int length=MultiByteToWideChar(CP_UTF8,0,value.data(),int(value.size()),nullptr,0);
    std::wstring result(size_t(length),L'\0');
    if(length) MultiByteToWideChar(CP_UTF8,0,value.data(),int(value.size()),result.data(),length);
    return result;
}
const wchar_t* keyTarget(AiProvider provider) {
    switch(provider) {
    case AiProvider::OpenRouter:return L"ShardBound.Editor.OpenRouter";
    case AiProvider::OpenAI:return L"ShardBound.Editor.OpenAI";
    case AiProvider::Anthropic:return L"ShardBound.Editor.Anthropic";
    case AiProvider::Google:return L"ShardBound.Editor.Google";
    default:throw std::runtime_error("Use o login do Codex para esta conexão");
    }
}
std::wstring readKey(AiProvider provider) {
    PCREDENTIALW raw=nullptr;
    if(!CredReadW(keyTarget(provider),CRED_TYPE_GENERIC,0,&raw)) throw std::runtime_error("Salve a chave do fornecedor primeiro");
    std::unique_ptr<CREDENTIALW,decltype(&CredFree)> credential(raw,CredFree);
    if(!credential->CredentialBlob || credential->CredentialBlobSize%sizeof(wchar_t)) throw std::runtime_error("Chave salva inválida");
    std::wstring key(reinterpret_cast<const wchar_t*>(credential->CredentialBlob),credential->CredentialBlobSize/sizeof(wchar_t));
    SecureZeroMemory(credential->CredentialBlob,credential->CredentialBlobSize);
    return key;
}
void check(BOOL success,const char* action) {
    if(!success) throw std::runtime_error(std::string(action)+" ("+std::to_string(GetLastError())+")");
}
void streamResponse(const wchar_t* host,const std::wstring& path,std::wstring headers,const Json& request,std::stop_token stop,
    const std::function<void(const Json&)>& onEvent) {
    if(stop.stop_requested()) throw std::runtime_error("Operação cancelada");
    Http session(WinHttpOpen(L"ShardBound Editor/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),WinHttpCloseHandle);
    check(bool(session),"Cannot open HTTP session");
    check(WinHttpSetTimeouts(session.get(),5000,5000,12000,45000),"Cannot set HTTP timeouts");
    Http connection(WinHttpConnect(session.get(),host,INTERNET_DEFAULT_HTTPS_PORT,0),WinHttpCloseHandle);
    check(bool(connection),"Cannot connect to provider");
    Http call(WinHttpOpenRequest(connection.get(),L"POST",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE),WinHttpCloseHandle);
    check(bool(call),"Cannot create HTTP request");
    DWORD options=WINHTTP_DISABLE_REDIRECTS;
    check(WinHttpSetOption(call.get(),WINHTTP_OPTION_DISABLE_FEATURE,&options,sizeof(options)),"Cannot disable redirects");
    auto body=request.dump();
    headers+=L"Content-Type: application/json\r\n";
    const auto sent=WinHttpSendRequest(call.get(),headers.c_str(),DWORD(headers.size()),body.data(),DWORD(body.size()),DWORD(body.size()),0);
    SecureZeroMemory(headers.data(),headers.size()*sizeof(wchar_t));check(sent,"Cannot send request to provider");
    check(WinHttpReceiveResponse(call.get(),nullptr),"Cannot receive provider response");
    DWORD code=0,bytes=sizeof(code);
    check(WinHttpQueryHeaders(call.get(),WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&bytes,WINHTTP_NO_HEADER_INDEX),"Cannot read HTTP status");
    if(code<200 || code>=300) throw std::runtime_error("Fornecedor retornou HTTP "+std::to_string(code));
    std::string result;
    size_t received=0;
    char buffer[8192];
    for(;;) {
        if(stop.stop_requested()) throw std::runtime_error("Operação cancelada");
        DWORD read=0;check(WinHttpReadData(call.get(),buffer,sizeof(buffer),&read),"Cannot read provider response");
        if(!read) break;
        received+=read;
        if(received>2*1024*1024) throw std::runtime_error("Provider response exceeds 2 MiB");
        result.append(buffer,read);
        size_t start=0;
        for(size_t end;(end=result.find('\n',start))!=std::string::npos;start=end+1) {
            auto line=std::string_view(result).substr(start,end-start);
            if(line.ends_with('\r')) line.remove_suffix(1);
            if(line.starts_with("data:")) {
                line.remove_prefix(5);
                if(line.starts_with(' ')) line.remove_prefix(1);
                if(!line.empty() && line!="[DONE]") onEvent(Json::parse(line));
            }
        }
        result.erase(0,start);
    }
}

constexpr auto instructions=R"(You are the authoring assistant in ShardBound Editor. Work on the selected map and local project only. Use the user's language. Reply with one JSON object, without markdown: {"message":"...","actions":[...]}. Request up to 32 actions per turn, then inspect their results and continue if necessary. Action forms:
{"tool":"read_map"}
{"tool":"read_catalog"}
{"tool":"reload_content"} (validate the staging mods after creating or changing content with the terminal)
{"tool":"set_block","x":1,"y":2,"z":3,"block":"mod:id"} (empty block means air)
{"tool":"fill","from":[x,y,z],"to":[x,y,z],"block":"mod:id"}
{"tool":"place_structure","id":"mod:id","x":1,"y":2,"z":3,"rotation":0}
{"tool":"set_spawn","x":1,"z":2}
{"tool":"place_model","id":"mod:id","x":1,"y":2,"z":3,"rotation":0,"scale":1}
{"tool":"link_interior","entrance":[x,z],"return":[x,z],"map":{"format":"shardbound-map","version":1,"bounds":[minX,minZ,width,depth],"spawn":[x,z],"exit":[x,z],"fills":[[x,y,z,width,height,depth,"mod:block"]]}}
{"tool":"test_map"} (runs a short local game preview against the current map and staging mods)
{"tool":"read_draft"} (reload preview.json after editing it with the terminal)
{"tool":"blockbench","name":"studio_read|read_project|studio_validate|create_elements|update_elements|studio_clip|studio_keyframes|paint_texture","args":{}}
{"tool":"terminal","command":"PowerShell command for local authoring/build/validation","risk":"low|high","reason":"impact","updates_map":false}
Use read-only terminal actions to inspect project files if needed. The terminal opens in the task directory. preview.json is the current map. content contains a staging copy of the installed mods. The editor validates each map action and presents the final changes for application. Do not ask the shell to obtain credentials. Return an empty actions array when done.)";

std::string ask(AiProvider provider,const std::string& model,const std::string& prompt,const AiStatus& status,std::stop_token stop) {
    if(model.empty() || model.size()>128) throw std::runtime_error("Selecione um ID de modelo");
    auto key=readKey(provider);
    std::wstring headers;
    if(provider==AiProvider::Anthropic) headers=L"x-api-key: "+key+L"\r\nanthropic-version: 2023-06-01\r\n";
    else if(provider==AiProvider::Google) headers=L"x-goog-api-key: "+key+L"\r\n";
    else headers=L"Authorization: Bearer "+key+L"\r\n";
    SecureZeroMemory(key.data(),key.size()*sizeof(wchar_t));
    std::string result;
    const auto add=[&](const std::string& delta) {
        if(delta.empty()) return;
        if(result.empty()) status("Recebendo resposta do fornecedor…");
        result+=delta;
    };
    const auto requireEvent=[](const Json& event) {
        if(event.contains("error") || event.value("type",std::string{})=="response.failed")
            throw std::runtime_error("O fornecedor interrompeu a geração");
    };
    bool completed=false;
    const auto finish=[&] {
        if(!completed) throw std::runtime_error("A conexão terminou antes da resposta completa");
        return result;
    };
    if(provider==AiProvider::OpenAI) {
        streamResponse(L"api.openai.com",L"/v1/responses",std::move(headers),
            {{"model",model},{"instructions",instructions},{"input",prompt},{"store",false},{"stream",true}},stop,[&](const Json& event) {
                requireEvent(event);
                const auto type=event.value("type",std::string{});
                if(type=="response.output_text.delta") add(event.at("delta"));
                if(type=="response.completed") completed=true;
            });
        return finish();
    }
    if(provider==AiProvider::OpenRouter) {
        streamResponse(L"openrouter.ai",L"/api/v1/chat/completions",std::move(headers),
            {{"model",model},{"stream",true},{"messages",Json::array({{{"role","system"},{"content",instructions}},{{"role","user"},{"content",prompt}}})}},
            stop,[&](const Json& event) {
                requireEvent(event);
                for(const auto& choice:event.value("choices",Json::array())) {
                    if(choice.contains("finish_reason") && choice.at("finish_reason")=="stop") completed=true;
                    const auto delta=choice.value("delta",Json::object());
                    if(delta.contains("content") && delta.at("content").is_string()) add(delta.at("content"));
                }
            });
        return finish();
    }
    if(provider==AiProvider::Anthropic) {
        streamResponse(L"api.anthropic.com",L"/v1/messages",std::move(headers),
            {{"model",model},{"stream",true},{"max_tokens",8192},{"system",instructions},{"messages",Json::array({{{"role","user"},{"content",prompt}}})}},
            stop,[&](const Json& event) {
                requireEvent(event);
                if(event.value("type",std::string{})=="content_block_delta" && event.at("delta").value("type",std::string{})=="text_delta")
                    add(event.at("delta").at("text"));
                if(event.value("type",std::string{})=="message_delta" && event.at("delta").value("stop_reason",std::string{})=="end_turn") completed=true;
            });
        return finish();
    }
    if(model.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.")!=std::string::npos)
        throw std::runtime_error("ID Gemini inválido");
    streamResponse(L"generativelanguage.googleapis.com",L"/v1beta/models/"+wide(model)+L":streamGenerateContent?alt=sse",std::move(headers),
        {{"systemInstruction",{{"parts",Json::array({{{"text",instructions}}})}}},
         {"contents",Json::array({{{"role","user"},{"parts",Json::array({{{"text",prompt}}})}}})},
         {"generationConfig",{{"responseMimeType","application/json"}}}},stop,[&](const Json& event) {
             requireEvent(event);
             for(const auto& candidate:event.value("candidates",Json::array())) {
                 if(candidate.value("finishReason",std::string{})=="STOP") completed=true;
                 if(!candidate.contains("content")) continue;
                 for(const auto& part:candidate.at("content").value("parts",Json::array()))
                     if(part.contains("text") && !part.value("thought",false)) add(part.at("text"));
             }
         });
    return finish();
}
}

bool editorAiHasKey(AiProvider provider) {
    if(provider==AiProvider::Codex) return false;
    PCREDENTIALW raw=nullptr;
    if(!CredReadW(keyTarget(provider),CRED_TYPE_GENERIC,0,&raw)) return false;
    CredFree(raw);return true;
}

void editorAiSaveKey(AiProvider provider,std::wstring key) {
    if(key.empty() || key.size()>1024 || key.find_first_of(L"\r\n\t ")!=std::wstring::npos) throw std::runtime_error("Chave inválida");
    CREDENTIALW credential{};credential.Type=CRED_TYPE_GENERIC;credential.TargetName=const_cast<LPWSTR>(keyTarget(provider));
    credential.CredentialBlobSize=DWORD(key.size()*sizeof(wchar_t));credential.CredentialBlob=reinterpret_cast<LPBYTE>(key.data());
    credential.Persist=CRED_PERSIST_LOCAL_MACHINE;
    const auto saved=CredWriteW(&credential,0);SecureZeroMemory(key.data(),key.size()*sizeof(wchar_t));
    check(saved,"Cannot save credential");
}

std::string runEditorAi(const EditorAiRequest& request,const AiTools& tools,const AiReview& review,const AiStatus& status,
    std::stop_token stop,const AiResponse& responseOverride) {
    if(request.prompt.empty() || request.prompt.size()>32000) throw std::runtime_error("Informe uma tarefa de até 32 mil bytes");
    std::string conversation="User task: "+request.prompt+"\nUse read_map and read_catalog to get project context.";
    std::string message;
    for(int turn=0;turn<16;++turn) {
        if(stop.stop_requested()) throw std::runtime_error("Operação cancelada");
        status("Aguardando resposta da IA…");
        std::string answer;
        if(responseOverride) answer=responseOverride(conversation);
        else if(request.provider==AiProvider::Codex) {
            const auto prompt=std::string(instructions)+"\n\n"+conversation;
            auto arguments=std::vector<std::wstring>{L"exec",L"--ignore-user-config",L"--skip-git-repo-check",L"--ephemeral",
                L"--sandbox",L"read-only",L"--json"};
            if(!request.model.empty()) {arguments.push_back(L"--model");arguments.push_back(wide(request.model));}
            arguments.push_back(L"-");
            const auto result=runEditorProcess(L"codex.exe",arguments,request.work,prompt,stop);
            if(result.exitCode) throw std::runtime_error("Codex terminou com código "+std::to_string(result.exitCode)+". Confira se o Codex está instalado e autenticado.");
            std::istringstream lines(result.output);std::string line;
            while(std::getline(lines,line)) {
                const auto event=Json::parse(line,nullptr,false);
                if(event.is_discarded() || !event.is_object() || event.value("type",std::string{})!="item.completed") continue;
                const auto& item=event.at("item");
                if(item.value("type",std::string{})=="agent_message") answer=item.at("text");
            }
        } else answer=ask(request.provider,request.model,conversation,status,stop);
        const auto start=answer.find('{'),end=answer.rfind('}');
        if(start==std::string::npos || end==std::string::npos) throw std::runtime_error("A IA não retornou um plano de ações válido");
        const auto response=Json::parse(answer.substr(start,end-start+1));
        const auto& actions=response.at("actions");
        if(!actions.is_array() || actions.size()>32) throw std::runtime_error("A resposta contém ações inválidas ou demais ações");
        message=response.value("message",std::string{});if(!message.empty()) status(message);
        if(actions.empty()) return message;
        auto results=Json::array();
        for(const auto& action:actions) {
            if(stop.stop_requested()) throw std::runtime_error("Operação cancelada");
            const auto name=action.at("tool").get<std::string>();
            try {
                if(name=="capture_terminal") throw std::runtime_error("Unknown editor tool");
                if(name=="terminal") {
                    const auto command=action.at("command").get<std::string>();
                    if(command.empty() || command.size()>8000) throw std::runtime_error("Comando inválido");
                    const auto reason=action.value("reason",std::string("Comando de terminal"));
                    const auto assessment=action.value("risk",std::string("unknown"));
                    if(!request.autonomous && !review(wide("Risco: "+assessment+"\n"+reason+"\n\n"+command))) {
                        results.push_back({{"tool",name},{"error","Autor recusou o comando"}});continue;
                    }
                    status("Terminal > "+command);
                    const auto script=std::wstring(L"[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)\n")+wide(command);
                    EditorProcessResult result;
                    try {result=runEditorProcess(L"powershell.exe",{L"-NoProfile",L"-NonInteractive",L"-Command",script},request.work,{},stop);}
                    catch(const std::exception&) {
                        if(stop.stop_requested()) {
                            try {(void)tools({{"tool","capture_terminal"},{"reload_map",action.value("updates_map",false)},
                                {"label","Cancelado · "+command.substr(0,100)}});}catch(const std::exception& error) {status(error.what());}
                        }
                        throw;
                    }
                    const auto output=result.output.substr(0,12000);
                    status("Terminal ("+std::to_string(result.exitCode)+")\n"+output);
                    results.push_back({{"tool",name},{"exitCode",result.exitCode},{"output",output}});
                    results.push_back({{"tool","capture_terminal"},{"result",tools({{"tool","capture_terminal"},
                        {"reload_map",action.value("updates_map",false)},{"label",command.substr(0,120)}})}});
                } else {
                    const bool risky=action.value("risk",std::string{})=="high" || name=="read_draft" || name=="blockbench" ||
                        (name=="fill" && action.value("block",std::string{}).empty());
                    if(!request.autonomous && risky && !review(wide(action.dump()))) {
                        results.push_back({{"tool",name},{"error","Autor recusou a alteração"}});continue;
                    }
                    results.push_back({{"tool",name},{"result",tools(action)}});
                }
            } catch(const std::exception& error) {
                if(stop.stop_requested()) throw;
                results.push_back({{"tool",name},{"error",error.what()}});
            }
        }
        const auto summary=results.dump(-1,' ',false,Json::error_handler_t::replace);
        if(conversation.size()+summary.size()+answer.size()>240000) throw std::runtime_error("O contexto da tarefa atingiu o limite");
        conversation+="\nAssistant actions: "+answer+"\nTool results: "+summary;
    }
    return "A tarefa atingiu o limite de etapas. Revise as alterações do rascunho.";
}
}
