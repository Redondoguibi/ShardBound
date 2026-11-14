#include "world/RegionEdits.h"
#include <fstream>
#include <sstream>
#include <charconv>
#include <cmath>
#include <limits>
#include "core/IoExecutor.h"
#include "core/AtomicFile.h"
#include <Windows.h>
#include <atomic>
namespace game {
namespace {
using Json=nlohmann::json;
std::int64_t coordinate(const Json& value) {
    const auto s=value.get<std::string>();std::int64_t n=0;auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),n);
    if(error!=std::errc{} || end!=s.data()+s.size() || std::abs(double(n))>1e12) throw std::runtime_error("Invalid terrain page coordinate");return n;
}
std::string contents(const std::filesystem::path& file) {
    if(std::filesystem::file_size(file)>4*1024*1024) throw std::runtime_error("Terrain page exceeds 4 MiB");
    std::ifstream in(file,std::ios::binary);if(!in) throw std::runtime_error("Cannot read terrain page: "+file.string());
    std::string text{std::istreambuf_iterator<char>(in),{}};if(in.bad()) throw std::runtime_error("Cannot read terrain page");return text;
}
BlockEdits decode(const std::string& text,RegionEdits::Key key) {
    const auto root=Json::parse(text);const auto& rows=root.is_array()?root:root.at("cells");
    if(!root.is_array() && (root.at("version")!=1 || !root.at("palette").is_array() || root.at("palette").size()>16*16*48)) throw std::runtime_error("Invalid terrain palette");
    if(!rows.is_array() || rows.size()>16*16*48) throw std::runtime_error("Invalid terrain page");
    BlockEdits edits;
    for(const auto& row:rows) {
        if(!row.is_array() || row.size()!=4 || !row[0].is_number_integer() || !row[1].is_number_integer() || !row[2].is_number_integer()) throw std::runtime_error("Invalid terrain page cell");
        std::string block;
        if(root.is_array()) block=row[3].get<std::string>();else { if(!row[3].is_number_unsigned()) throw std::runtime_error("Invalid terrain palette index");block=root.at("palette").at(row[3].get<size_t>()).get<std::string>(); }
        if(row[0]<0 || row[0]>=16 || row[1]<0 || row[1]>=48 || row[2]<0 || row[2]>=16) throw std::runtime_error("Terrain cell coordinate out of range");
        const int x=row[0].get<int>(),y=row[1].get<int>(),z=row[2].get<int>();
        if(x<0 || x>=16 || z<0 || z>=16 || y<0 || y>=48 || std::abs(double(key.first*16+x))>1e12 || std::abs(double(key.second*16+z))>1e12 || !edits.emplace(std::tuple{key.first*16+x,y,key.second*16+z},std::move(block)).second) throw std::runtime_error("Invalid/duplicate terrain page cell");
    }
    return edits;
}
}
RegionEdits::RegionEdits(std::filesystem::path directory):directory_(std::move(directory)) {}
void RegionEdits::directory(std::filesystem::path path) {
    if(!pages_.empty() && path!=directory_) throw std::runtime_error("Cannot change terrain storage while populated");directory_=std::move(path);
}
void RegionEdits::flush(Page& page) const {
    if(page.pending) {
        const auto pending=page.pending;
        try {(void)pending->future.get();}catch(const std::exception&) {} // Retain/retry failed data rather than discard edits.
        std::shared_ptr<const BlockEdits> retryData;
        {std::lock_guard lock(pending->mutex);page.file=pending->file;retryData=pending->data;}
        if(page.file.empty()) {
            if(!retryData) throw std::runtime_error("Pending terrain write has neither data nor published file");
            Page retry;retry.data=std::move(retryData);flush(retry);page.file=retry.file;
            std::lock_guard lock(pending->mutex);pending->file=page.file;pending->data.reset();
        }
        page.pending.reset();
    }
    if(page.removed || !page.file.empty() || !page.data) return;
    const auto data=page.data;
    std::lock_guard lock(*io_);
    if(directory_.empty()) throw std::runtime_error("Terrain storage directory is not configured");
    Json rows=Json::array(),palette=Json::array();std::map<std::string,size_t> ids;
    for(const auto& [cell,block]:*data) { const auto [x,y,z]=cell;auto [it,added]=ids.emplace(block,ids.size());if(added) palette.push_back(block);rows.push_back({x-floorDiv(x,16)*16,y,z-floorDiv(z,16)*16,it->second}); }
    const auto text=Json{{"version",1},{"palette",std::move(palette)},{"cells",std::move(rows)}}.dump();if(text.size()>4*1024*1024) throw std::runtime_error("Terrain page exceeds 4 MiB");std::ostringstream name;name<<std::hex<<seedHash(text)<<"-"<<text.size()<<".json";const auto filename=name.str();
    std::filesystem::create_directories(directory_);const auto file=directory_/filename;
    if(std::filesystem::exists(file)) { if(contents(file)!=text) throw std::runtime_error("Terrain page hash collision/corruption"); }
    else {
        atomicWrite(file,text);
    }
    page.file=filename;
}
size_t RegionEdits::residentPages() const { return resident_.size(); }
size_t RegionEdits::pendingWrites() const {
    size_t count=0;for(const auto& [key,page]:pages_) {(void)key;if(page.pending) {std::lock_guard lock(page.pending->mutex);if(page.pending->data) ++count;}}return count;
}
void RegionEdits::trim(Key keep,size_t limit) const {
    if(directory_.empty()) return;
    while(resident_.size()>limit) {
        auto victim=resident_.end();for(auto it=resident_.begin();it!=resident_.end();++it) if(*it!=keep && (victim==resident_.end() || pages_.at(*it).used<pages_.at(*victim).used)) victim=it;
        if(victim==resident_.end()) break;auto& page=pages_.at(*victim);
        if(page.file.empty() && !page.pending && !page.removed) {
            RegionEdits store(directory_);store.io_=io_;auto pending=std::make_shared<PendingWrite>();pending->data=page.data;
            pending->future=IoExecutor::instance().submit([store=std::move(store),weak=std::weak_ptr<PendingWrite>(pending),data=page.data]() mutable {
                Page item;item.data=std::move(data);store.flush(item);
                if(const auto state=weak.lock()) {std::lock_guard lock(state->mutex);state->file=item.file;state->data.reset();}return item.file;
            }).share();page.pending=std::move(pending);
        }
        page.data.reset();resident_.erase(victim);
    }
}
const BlockEdits& RegionEdits::read(Key key) const {
    static const BlockEdits empty;const auto it=pages_.find(key);if(it==pages_.end()) return empty;
    auto& page=it->second;if(!page.data) { flush(page); auto data=std::make_shared<const BlockEdits>(decode([&]{ auto text=contents(directory_/page.file);std::ostringstream name;name<<std::hex<<seedHash(text)<<"-"<<text.size()<<".json";if(name.str()!=page.file) throw std::runtime_error("Terrain page checksum mismatch");return text; }(),key));resident_.insert(key);page.data=std::move(data); }
    page.used=++clock_;trim(key);return *page.data;
}
BlockEdits RegionEdits::chunk(Key key) const { return read(key); }
BlockEdits RegionEdits::neighborhood(Key key) const {
    BlockEdits result;const auto left=key.first*16-1,right=key.first*16+16,top=key.second*16-1,bottom=key.second*16+16;
    for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) {
        const auto& page=read({key.first+x,key.second+z});
        for(auto it=page.lower_bound({left,0,std::numeric_limits<std::int64_t>::min()});it!=page.end() && std::get<0>(it->first)<=right;++it) if(std::get<2>(it->first)>=top && std::get<2>(it->first)<=bottom) result.insert(*it);
    }
    return result;
}
BlockEdits RegionEdits::all() const { BlockEdits result;for(const auto& [key,page]:pages_) { (void)page;const auto& data=read(key);result.insert(data.begin(),data.end()); }return result; }
std::optional<std::string> RegionEdits::get(std::int64_t x,int y,std::int64_t z) const { const auto& page=read({floorDiv(x,16),floorDiv(z,16)});auto it=page.find({x,y,z});if(it==page.end()) return {};return it->second; }
void RegionEdits::replace(Key key,BlockEdits edits) { if(edits.empty()) { pages_.erase(key);resident_.erase(key); }else stage(key,std::move(edits)); }
void RegionEdits::stage(Key key,BlockEdits edits) {
    auto data=std::make_shared<const BlockEdits>(std::move(edits));auto [it,created]=pages_.try_emplace(key);
    try { resident_.insert(key); }catch(...) { if(created) pages_.erase(it);throw; }
    const bool removed=data->empty();it->second={std::move(data),{},++clock_,removed};trim(key);
}
void RegionEdits::preparePublish(const RegionEdits& patch) const { if(!directory_.empty()) trim({std::numeric_limits<std::int64_t>::min(),0},64-patch.resident_.size()); }
void RegionEdits::publish(RegionEdits patch) noexcept {
    while(!patch.pages_.empty()) {
        auto node=patch.pages_.extract(patch.pages_.begin());const auto key=node.key();
        if(node.mapped().removed) { pages_.erase(key);resident_.erase(key);patch.resident_.erase(key);continue; }
        node.mapped().used=++clock_;
        auto old=pages_.find(key);if(old!=pages_.end()) { std::swap(old->second,node.mapped());resident_.erase(key); }else pages_.insert(std::move(node));
        if(patch.resident_.contains(key)) resident_.insert(patch.resident_.extract(key));
    }
}
nlohmann::json RegionEdits::snapshot() const {
    Json entries=Json::array();for(auto& [key,page]:pages_) { flush(page);entries.push_back({std::to_string(key.first),std::to_string(key.second),page.file}); }
    return {{"version",1},{"pages",std::move(entries)}};
}
RegionEdits RegionEdits::window(Key key) const {
    RegionEdits result(directory_);result.io_=io_;
    for(int x=-1;x<=1;++x) for(int z=-1;z<=1;++z) {
        const Key k{key.first+x,key.second+z};const auto it=pages_.find(k);if(it==pages_.end()) continue;
        result.pages_.emplace(k,it->second);if(it->second.data) result.resident_.insert(k);
    }
    return result;
}
namespace {
void validFile(const std::string& file,bool index=false) {
    if(file.empty() || file.size()>64 || file.find_first_not_of("0123456789abcdef-.jsonidx")!=std::string::npos || file.find("..")!=std::string::npos || !file.ends_with(index?".index":".json")) throw std::runtime_error("Invalid terrain filename");
}
void hashText(std::uint64_t& hash,std::string_view text) {for(unsigned char c:text) {hash^=c;hash*=1099511628211ULL;}}
std::string hashedName(std::uint64_t hash,std::uintmax_t size,const char* suffix) {std::ostringstream name;name<<std::hex<<hash<<"-"<<size<<suffix;return name.str();}
template<class F> void visitIndex(const std::filesystem::path& directory,const Json& manifest,F visit) {
    if(manifest.at("version")==1) {
        if(!manifest.at("pages").is_array()) throw std::runtime_error("Invalid terrain manifest");
        for(const auto& row:manifest.at("pages")) visit(row);return;
    }
    if(manifest.at("version")!=2 || !manifest.at("count").is_number()) throw std::runtime_error("Invalid terrain index version/count");
    const auto n=manifest.at("count").get<double>();if(!std::isfinite(n) || n<0 || n>10000000 || n!=std::floor(n)) throw std::runtime_error("Invalid terrain index count");
    const auto count=static_cast<std::uint64_t>(n);if(count>10000000) throw std::runtime_error("Terrain index exceeds 10 million pages");
    const auto name=manifest.at("index").get<std::string>();validFile(name,true);
    std::ifstream input(directory/name,std::ios::binary);if(!input) throw std::runtime_error("Terrain index missing");
    std::uint64_t hash=14695981039346656037ULL;std::uintmax_t bytes=0;std::string line;std::uint64_t rows=0;char buffer[514];
    while(input.getline(buffer,sizeof(buffer))) {
        line=buffer;
        if(line.empty() || line.size()>512 || ++rows>count) throw std::runtime_error("Invalid terrain index row/count");
        hashText(hash,line);hashText(hash,"\n");bytes+=line.size()+1;visit(Json::parse(line));
    }
    if(!input.eof() || rows!=count || name!=hashedName(hash,bytes,".index")) throw std::runtime_error("Terrain index checksum/count mismatch");
}
}
nlohmann::json RegionEdits::checkpoint() const {
    if(pages_.size()>10000000) throw std::runtime_error("Terrain index exceeds 10 million pages");
    static std::atomic<std::uint64_t> serial=0;std::filesystem::create_directories(directory_);
    const auto temp=directory_/("index.pending-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++serial));
    std::ofstream output(temp,std::ios::binary);if(!output) throw std::runtime_error("Cannot create terrain index");
    std::uint64_t hash=14695981039346656037ULL;std::uintmax_t bytes=0;
    for(auto& [key,page]:pages_) {
        flush(page);const auto row=Json::array({std::to_string(key.first),std::to_string(key.second),page.file}).dump()+"\n";
        hashText(hash,row);bytes+=row.size();output.write(row.data(),std::streamsize(row.size()));
    }
    output.close();if(!output) throw std::runtime_error("Cannot write terrain index");flushFile(temp);
    const auto name=hashedName(hash,bytes,".index");
    {std::lock_guard lock(*io_);if(std::filesystem::exists(directory_/name)) {
        visitIndex(directory_,{{"version",2},{"index",name},{"count",pages_.size()}},[](const Json&){});std::filesystem::remove(temp);
    }else publishFile(temp,directory_/name);}
    return {{"version",2},{"index",name},{"count",pages_.size()}};
}
std::vector<std::string> RegionEdits::referencedFiles(const std::filesystem::path& directory,const Json& manifest) {
    std::vector<std::string> result;
    if(manifest.at("version")==2) {const auto file=manifest.at("index").get<std::string>();validFile(file,true);result.push_back(file);}
    visitIndex(directory,manifest,[&](const Json& row){if(!row.is_array() || row.size()!=3) throw std::runtime_error("Invalid terrain manifest row");const auto file=row[2].get<std::string>();validFile(file);result.push_back(file);});return result;
}
RegionEdits RegionEdits::restore(const std::filesystem::path& directory,const Json& manifest,const WorldDefinition& definition) {
    RegionEdits result(directory);
    visitIndex(directory,manifest,[&](const Json& row){
        if(!row.is_array() || row.size()!=3) throw std::runtime_error("Invalid terrain manifest entry");
        const Key key{coordinate(row[0]),coordinate(row[1])};if(std::abs(double(key.first))>62500000000.0 || std::abs(double(key.second))>62500000000.0) throw std::runtime_error("Terrain page outside world");
        const auto file=row[2].get<std::string>();validFile(file);
        if(!result.pages_.emplace(key,Page{nullptr,file,0}).second) throw std::runtime_error("Duplicate terrain page");
        for(const auto& [cell,block]:result.read(key)) {(void)cell;if(!block.empty() && !definition.blocks.contains(block)) throw std::runtime_error("Unknown saved terrain block: "+block);}
    });return result;
}
}
