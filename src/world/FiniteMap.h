#pragma once
#include <nlohmann/json_fwd.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace game {
struct WorldDefinition;

class FiniteMap {
public:
    static constexpr int height = 48;
    static constexpr int maxSide = 512;
    static constexpr int coordinateLimit = 1000000;
    struct Bounds {
        int minX, minZ, width, depth;
    };
    struct Spawn { int x,z; bool operator==(const Spawn&) const = default; };
    struct Interior {
        Spawn entrance,returnPoint,exit;
        std::shared_ptr<const FiniteMap> map;
    };
    struct Area {
        const FiniteMap* map;
        int offsetX,offsetZ;
        Bounds bounds() const;
    };
    struct Door { Spawn from,to; };
    struct Prop { std::string model;int x,y,z;float yaw,scale; };

    explicit FiniteMap(Bounds bounds);
    static FiniteMap parse(const nlohmann::json& json, const WorldDefinition& catalog);
    static FiniteMap parseDraft(const nlohmann::json& json, const WorldDefinition& catalog);
    static FiniteMap read(const std::filesystem::path& path, const WorldDefinition& catalog);
    nlohmann::json json() const;
    void save(const std::filesystem::path& path) const;

    Bounds bounds() const { return bounds_; }
    Spawn spawn() const { return spawn_; }
    void setSpawn(int x,int z);
    void setExit(std::optional<Spawn> exit);
    const std::optional<Spawn>& exit() const { return exit_; }
    void addInterior(Interior interior);
    void removeInterior();
    const std::vector<Interior>& interiors() const { return interiors_; }
    std::vector<Door> doors() const;
    std::optional<Area> areaAt(std::int64_t x,std::int64_t z) const;
    std::vector<Area> areas() const;
    void addProp(Prop prop);
    void removeProp();
    const std::vector<Prop>& props() const { return props_; }
    void validateDoors(const WorldDefinition& catalog) const;
    bool contains(std::int64_t x, std::int64_t z) const;
    std::string_view block(std::int64_t x, int y, std::int64_t z) const;
    void setBlock(int x, int y, int z, const std::string& id, const WorldDefinition& catalog);
    void placeStructures(const nlohmann::json& placements,const WorldDefinition& catalog);
    std::string biome(std::int64_t x, std::int64_t z) const;

private:
    struct Zone {
        int x, z, width, depth;
        std::string id;
    };
    size_t index(int x, int y, int z) const;
    std::uint16_t paletteIndex(const std::string& id, const WorldDefinition& catalog);

    Bounds bounds_;
    Spawn spawn_{};
    std::vector<std::string> palette_{std::string{}};
    std::vector<std::uint16_t> blocks_;
    std::vector<Zone> zones_;
    std::optional<Spawn> exit_;
    std::vector<Interior> interiors_;
    std::vector<Prop> props_;
    Area interiorArea(size_t index) const;
    static FiniteMap parseMap(const nlohmann::json& json,const WorldDefinition& catalog,bool allowInteriors,bool playable);
};
}
