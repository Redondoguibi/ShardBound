#include "platform/UserPaths.h"
#include <Windows.h>
#include <ShlObj.h>
#include <memory>
#include <stdexcept>
namespace game {
std::filesystem::path defaultUserDirectory(const std::filesystem::path& installation,bool portable) {
    const auto legacy=installation/"user";
    if(portable || std::filesystem::is_directory(legacy)) return legacy;
    PWSTR directory=nullptr;const auto result=SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&directory);
    std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> memory(directory,CoTaskMemFree);
    if(FAILED(result) || !directory) throw std::runtime_error("Cannot locate user data; specify --user-dir DIRECTORY");
    return std::filesystem::path(directory)/"ShardBound";
}
}
