#pragma once
#include <filesystem>
#include <map>
#include <string>
namespace game {
class VirtualFileSystem {
public:
    void mount(const std::string& nameSpace, const std::filesystem::path& root);
    void swap(VirtualFileSystem& other) noexcept { mounts_.swap(other.mounts_); }
    void unmount(const std::string& nameSpace);
    std::filesystem::path resolve(const std::string& nameSpace, const std::filesystem::path& relative) const;
    static std::filesystem::path contained(const std::filesystem::path& root, const std::filesystem::path& relative);
private:
    std::map<std::string,std::filesystem::path> mounts_;
};
}
