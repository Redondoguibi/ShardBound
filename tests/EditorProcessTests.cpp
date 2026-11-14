#include "tools/EditorProcess.h"
#include "tools/EditorAi.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

int main(int argc,char**) {
    try {
        const auto directory=std::filesystem::temp_directory_path();
        const auto result=game::runEditorProcess(L"powershell.exe",{L"-NoProfile",L"-NonInteractive",L"-Command",
            L"$value = [Console]::In.ReadToEnd(); [Console]::WriteLine($value.Length); [Console]::Error.WriteLine('diagnostic'); exit 7"},
            directory,std::string(24000,'x'),{});
        if(result.exitCode!=7 || result.output.find("24000")==std::string::npos || result.output.find("diagnostic")==std::string::npos)
            throw std::runtime_error("Terminal did not preserve input, output and exit code");
        std::stop_source stop;
        std::jthread cancel([&] {std::this_thread::sleep_for(std::chrono::milliseconds(300));stop.request_stop();});
        bool canceled=false;
        try {
            (void)game::runEditorProcess(L"powershell.exe",{L"-NoProfile",L"-NonInteractive",L"-Command",L"Start-Sleep -Seconds 30"},
                directory,std::string(24000,'x'),stop.get_token());
        } catch(const std::runtime_error&) {canceled=stop.stop_requested();}
        if(!canceled) throw std::runtime_error("Terminal did not cancel");
        using Json=nlohmann::json;
        game::EditorAiRequest request;request.prompt="Crie uma coluna";request.work=directory;
        unsigned approvals=0,turn=0,snapshots=0,writes=0;
        const auto final=game::runEditorAi(request,[&](const Json& action) -> Json {
            if(action.at("tool")=="capture_terminal") ++snapshots;
            else if(action.at("tool")=="set_block") ++writes;
            else throw std::runtime_error("Unexpected tool");
            return {{"ok",true}};
        },[&](const std::wstring&) {return ++approvals==2;},[](const std::string&) {},{},[&](const std::string& history) {
            if(turn++==0) return std::string(R"({"actions":[{"tool":"terminal","command":"Write-Output denied"},{"tool":"terminal","command":"Write-Output editor-terminal-ok"},{"tool":"set_block"}]})");
            if(history.find("Autor recusou o comando")==std::string::npos || history.find("editor-terminal-ok")==std::string::npos ||
                history.find("capture_terminal")==std::string::npos) throw std::runtime_error("Tool results missing from follow-up");
            return std::string(R"({"message":"done","actions":[]})");
        });
        if(final!="done" || approvals!=2 || snapshots!=1 || writes!=1) throw std::runtime_error("AI approval and tool loop failed");
        if(argc==2) {
            request.prompt="Teste de integração. Faça exatamente uma ação set_block com x=1, y=2, z=3 e block=plains:stone. Depois termine.";
            unsigned edits=0;
            (void)game::runEditorAi(request,[&](const Json& action) -> Json {
                if(action.at("tool")=="set_block" && action.at("x")==1 && action.at("y")==2 && action.at("z")==3 &&
                    action.at("block")=="plains:stone") ++edits;
                return {{"ok",true}};
            },[](const std::wstring&) {return false;},[](const std::string& status) {std::cout<<status<<'\n';},{});
            if(edits!=1) throw std::runtime_error("Live Codex did not invoke the requested map tool");
            std::cout<<"PASS Codex local: map tool and continuation\n";
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
