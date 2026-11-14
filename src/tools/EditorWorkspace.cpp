#include "tools/EditorWorkspace.h"
#include <Windows.h>
#include <ShlObj.h>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace game {
std::filesystem::path createEditorRevision(std::string_view label) {
    PWSTR directory=nullptr;
    const auto result=SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&directory);
    const std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> owner(directory,CoTaskMemFree);
    if(FAILED(result) || !directory) throw std::runtime_error("Não foi possível localizar os dados locais do editor");
    const auto root=std::filesystem::path(directory)/"ShardBound"/"Editor"/"Revisions";
    std::filesystem::create_directories(root);
    static std::atomic<unsigned> serial{};
    for(int attempt=0;attempt<4;++attempt) {
        const auto revision=root/(std::string(label)+"-"+std::to_string(GetCurrentProcessId())+"-"+
            std::to_string(GetTickCount64())+"-"+std::to_string(++serial));
        if(std::filesystem::create_directory(revision)) return revision;
    }
    throw std::runtime_error("Não foi possível criar uma revisão do editor");
}
}
