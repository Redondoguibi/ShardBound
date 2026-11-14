#pragma once
#include "world/WorldDefinition.h"
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <future>
#include <mutex>
namespace game {
using BlockEdits=std::map<std::tuple<std::int64_t,int,std::int64_t>,std::string>;
// Immutable checkpoint pages; resident voxel data is limited independently of exploration.
class RegionEdits {
public:
    using Key=std::pair<std::int64_t,std::int64_t>;
    explicit RegionEdits(std::filesystem::path directory={});
    void directory(std::filesystem::path path);
    BlockEdits chunk(Key key) const;
    BlockEdits neighborhood(Key key) const;
    BlockEdits all() const;
    std::optional<std::string> get(std::int64_t x,int y,std::int64_t z) const;
    void replace(Key key,BlockEdits edits);
    void stage(Key key,BlockEdits edits);
    void preparePublish(const RegionEdits& patch) const;
    void publish(RegionEdits patch) noexcept;
    nlohmann::json snapshot() const;
    nlohmann::json checkpoint() const;
    RegionEdits window(Key key) const;
    static std::vector<std::string> referencedFiles(const std::filesystem::path& directory,const nlohmann::json& manifest);
    static RegionEdits restore(const std::filesystem::path& directory,const nlohmann::json& manifest,const WorldDefinition& definition);
    size_t residentPages() const;
    size_t pendingWrites() const;
    size_t pageCount() const { return pages_.size(); }
private:
    struct PendingWrite {std::mutex mutex;std::shared_future<std::string> future;std::shared_ptr<const BlockEdits> data;std::string file;};
    struct Page { std::shared_ptr<const BlockEdits> data;std::string file;std::uint64_t used=0;bool removed=false;std::shared_ptr<PendingWrite> pending; };
    const BlockEdits& read(Key key) const;
    void trim(Key keep,size_t limit=64) const;
    void flush(Page& page) const;
    std::filesystem::path directory_;
    mutable std::map<Key,Page> pages_;
    mutable std::set<Key> resident_;
    mutable std::uint64_t clock_=0;
    std::shared_ptr<std::mutex> io_=std::make_shared<std::mutex>();
};
}
