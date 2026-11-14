#pragma once
#include "core/Diagnostic.h"
#include <array>
#include <filesystem>
#include <vector>
namespace game {
struct Version {
    std::array<unsigned,3> parts{};
    static Version parse(std::string_view text);
    auto operator<=>(const Version&) const = default;
};
struct Dependency { std::string id, operation; Version version; bool optional = false; bool accepts(Version candidate) const; };
struct ModManifest {
    std::string id,name,entry,native;
    Version version;
    std::filesystem::path root;
    std::vector<Dependency> dependencies;
    bool dependencyCycle = false;
    unsigned saveSchema=1;
    std::string compatibleFrom;
    bool acceptsSave(const std::string& saved,unsigned schema) const;
};
ModManifest readManifest(const std::filesystem::path& root);
struct Discovery { std::vector<ModManifest> mods; std::vector<Diagnostic> diagnostics; };
Discovery discoverMods(const std::filesystem::path& directory);
std::vector<ModManifest> resolveMods(std::vector<ModManifest> mods,std::vector<Diagnostic>& diagnostics);
}
