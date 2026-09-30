#include "assets/VirtualFileSystem.h"
#include "assets/AssetId.h"
#include "core/Diagnostic.h"
namespace game {
std::filesystem::path VirtualFileSystem::contained(const std::filesystem::path& root, const std::filesystem::path& relative) {
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.native().find(L':') != std::wstring::npos)
        throw DiagnosticError({"ASSET102","Source must be a relative path",relative.string()});
    const auto canonicalRoot = std::filesystem::canonical(root);
    const auto result = std::filesystem::weakly_canonical(canonicalRoot / relative);
    auto r = canonicalRoot.begin(), p = result.begin();
    for (;r != canonicalRoot.end();++r,++p) {
        if (p == result.end() || *r != *p) throw DiagnosticError({"ASSET103","Path escapes mounted root",relative.string()});
    }
    if (!std::filesystem::is_regular_file(result)) throw DiagnosticError({"ASSET104","Source file missing",result.string()});
    return result;
}
void VirtualFileSystem::mount(const std::string& nameSpace, const std::filesystem::path& root) {
    if (!validNamespace(nameSpace) || !std::filesystem::is_directory(root)) throw DiagnosticError({"ASSET105","Invalid namespace or mount directory",root.string(),nameSpace});
    if (!mounts_.emplace(nameSpace,std::filesystem::canonical(root)).second) throw DiagnosticError({"ASSET106","Namespace already mounted",root.string(),nameSpace});
}
void VirtualFileSystem::unmount(const std::string& nameSpace) { mounts_.erase(nameSpace); }
std::filesystem::path VirtualFileSystem::resolve(const std::string& nameSpace, const std::filesystem::path& relative) const {
    const auto it = mounts_.find(nameSpace);
    if (it == mounts_.end()) throw DiagnosticError({"ASSET107","Namespace not mounted",relative.string(),nameSpace});
    return contained(it->second,relative);
}
}
