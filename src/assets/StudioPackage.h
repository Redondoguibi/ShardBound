#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <memory>
namespace game {
struct StudioPackage {
    std::filesystem::path root;
    nlohmann::json manifest,effects,events,player;
    std::filesystem::path file(const std::string& relative) const;
    const nlohmann::json* clip(const std::string& name) const;
    const nlohmann::json* effect(const std::string& id) const;
};
std::shared_ptr<StudioPackage> loadStudioPackage(const std::filesystem::path& manifest,const std::filesystem::path& cache,const std::string& nameSpace="");
}
