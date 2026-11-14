#include "world/WorldStorage.h"
#include "world/RegionEdits.h"
#include "persistence/SaveGame.h"
#include "assets/Fingerprint.h"
#include "assets/VirtualFileSystem.h"
#include "core/AtomicFile.h"
#include "core/IoExecutor.h"
#include <Windows.h>
#include <fstream>
#include <set>
#include <chrono>
#include <atomic>
namespace game {
namespace {
using Json=nlohmann::json;
void identifier(const std::string& id) {if(id.empty() || id.size()>64 || id.find_first_not_of("0123456789-")!=std::string::npos) throw std::runtime_error("Invalid world/backup identifier");}
void regularTree(const std::filesystem::path& path) {const auto flags=GetFileAttributesW(path.c_str());if(flags==INVALID_FILE_ATTRIBUTES || (flags&FILE_ATTRIBUTE_REPARSE_POINT)) throw std::runtime_error("World maintenance refuses reparse points: "+path.string());}
void within(const std::filesystem::path& root,const std::filesystem::path& target) {
    const auto base=std::filesystem::weakly_canonical(root),resolved=std::filesystem::weakly_canonical(target);
    auto r=base.begin(),p=resolved.begin();for(;r!=base.end();++r,++p) if(p==resolved.end() || *r!=*p) throw std::runtime_error("World maintenance target escapes user directory");
    if(p==resolved.end()) throw std::runtime_error("Maintenance target cannot equal root");
}
// Every recursive deletion resolves and verifies the target against its explicit maintenance root.
void removeTree(const std::filesystem::path& root,const std::filesystem::path& target) {
    if(!std::filesystem::exists(target)) return;
    const auto base=std::filesystem::canonical(root),resolved=std::filesystem::canonical(target);
    auto r=base.begin(),p=resolved.begin();for(;r!=base.end();++r,++p) if(p==resolved.end() || *r!=*p) throw std::runtime_error("Maintenance path escapes workspace");
    if(p==resolved.end()) throw std::runtime_error("Cannot remove maintenance root");
    regularTree(target);for(const auto& entry:std::filesystem::recursive_directory_iterator(target)) regularTree(entry.path());std::filesystem::remove_all(target);
}
std::vector<std::filesystem::path> files(const std::filesystem::path& root) {
    regularTree(root);std::vector<std::filesystem::path> result;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
        regularTree(entry.path());if(!entry.is_regular_file()) continue;
        const auto relative=entry.path().lexically_relative(root);const auto name=entry.path().filename().string();
        if(name.starts_with(".archive.") || name.find(".pending-")!=std::string::npos || name.ends_with(".tmp")) continue;
        result.push_back(relative);
    }
    std::sort(result.begin(),result.end());return result;
}
std::set<std::string> references(const std::filesystem::path& root,const WorldDefinition& definition) {
    std::set<std::string> used;
    for(const auto& entry:std::filesystem::directory_iterator(root)) {
        if(!entry.is_regular_file()) continue;const auto name=entry.path().filename().string();
        if(name=="world.json" || name=="world.json.bak" || (!name.ends_with(".json") && !name.ends_with(".json.bak"))) continue;
        const auto save=Json::parse(encodeData(SaveGame::inspect(entry.path())));
        if(save.at("state").contains("core") && save.at("state").at("core").contains("regions")) {
            const auto& manifest=save.at("state").at("core").at("regions");
            (void)RegionEdits::restore(root/"terrain",manifest,definition);
            for(const auto& file:RegionEdits::referencedFiles(root/"terrain",manifest)) used.insert(file);
        }
    }
    return used;
}
void copyTree(const std::filesystem::path& source,const std::filesystem::path& target) {
    for(const auto& path:files(source)) {
        const auto from=VirtualFileSystem::contained(source,path);std::filesystem::create_directories((target/path).parent_path());
        std::filesystem::copy_file(from,target/path,std::filesystem::copy_options::overwrite_existing);flushFile(target/path);
    }
}
Json archiveInfo(const std::filesystem::path& archive) {
    const auto marker=VirtualFileSystem::contained(archive,".archive.json");if(std::filesystem::file_size(marker)>65536) throw std::runtime_error("Backup marker too large");
    std::ifstream input(marker);Json result;input>>result;
    if(!input || result.at("version")!=1) throw std::runtime_error("Invalid world backup");
    if(fingerprint(VirtualFileSystem::contained(archive,".archive.index"),"world-backup-v1")!=result.at("index_hash").get<std::string>()) throw std::runtime_error("Backup index checksum mismatch");
    return result;
}
void validateArchive(const std::filesystem::path& archive,const std::string& id) {
    regularTree(archive);const auto info=archiveInfo(archive);if(info.at("id")!=id) throw std::runtime_error("Backup belongs to another world");
    std::ifstream input(archive/".archive.index",std::ios::binary);std::string line;std::set<std::string> seen;
    char buffer[4098];while(input.getline(buffer,sizeof(buffer))) {
        line=buffer;
        if(line.size()>4096) throw std::runtime_error("Backup index row too large");const auto row=Json::parse(line);const auto relative=row.at("file").get<std::string>();
        if(!seen.insert(relative).second) throw std::runtime_error("Duplicate backup file");
        const auto path=VirtualFileSystem::contained(archive,relative);regularTree(path);
        if(std::filesystem::file_size(path)!=row.at("size").get<std::uintmax_t>() || fingerprint(path,"world-backup-v1")!=row.at("hash").get<std::string>()) throw std::runtime_error("Backup file corrupted: "+relative);
    }
    if(!input.eof() || seen.size()!=info.at("files").get<size_t>() || files(archive).size()!=seen.size()) throw std::runtime_error("Backup file inventory mismatch");
}
}
void WorldStorage::recover(const std::string& id) const {
    identifier(id);const auto journal=directory_/("restore-"+id+".json");if(!std::filesystem::exists(journal)) return;
    within(directory_,directory_/id);
    const auto live=directory_/id,old=directory_/(id+".restore-old"),stage=directory_/(id+".restore-stage");
    if(!std::filesystem::exists(live)) {
        if(!std::filesystem::exists(old)) throw std::runtime_error("Interrupted restore has no recoverable world");
        regularTree(old);renameDirectory(old,live);
    }
    removeTree(directory_,old);removeTree(directory_,stage);std::filesystem::remove(journal);
}
std::vector<std::string> WorldStorage::backups(const std::string& id) const {
    identifier(id);const auto root=directory_.parent_path()/"world-backups"/id;within(directory_.parent_path(),root);std::vector<std::string> result;
    if(std::filesystem::exists(root)) for(const auto& entry:std::filesystem::directory_iterator(root)) {
        const auto name=entry.path().filename().string();if(entry.is_directory() && !name.ends_with(".pending") && std::filesystem::exists(entry.path()/".archive.json")) {identifier(name);result.push_back(name);}
    }
    std::sort(result.begin(),result.end());return result;
}
std::string WorldStorage::backup(const std::string& id) const {
    IoExecutor::instance().submit([]{}).get();
    const auto record=read(id);const auto source=directory(id);(void)references(source,record.definition);
    static std::atomic<unsigned> serial=0;const auto name=std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial);
    const auto root=directory_.parent_path()/"world-backups"/id,stage=root/(name+".pending"),target=root/name;
    within(directory_.parent_path(),stage);std::filesystem::create_directories(stage);copyTree(source,stage);
    std::ofstream index(stage/".archive.index",std::ios::binary);size_t count=0;
    for(const auto& path:files(stage)) {index<<Json{{"file",path.generic_string()},{"size",std::filesystem::file_size(stage/path)},{"hash",fingerprint(stage/path,"world-backup-v1")}}.dump()<<'\n';++count;}
    index.close();if(!index) throw std::runtime_error("Cannot write backup inventory");flushFile(stage/".archive.index");
    atomicWrite(stage/".archive.json",Json{{"version",1},{"id",id},{"files",count},{"index_hash",fingerprint(stage/".archive.index","world-backup-v1")}}.dump());
    validateArchive(stage,id);renameDirectory(stage,target);return name;
}
void WorldStorage::restoreBackup(const std::string& id,const std::string& backupId) const {
    IoExecutor::instance().submit([]{}).get();identifier(id);identifier(backupId);recover(id);
    const auto archive=directory_.parent_path()/"world-backups"/id/backupId;within(directory_.parent_path(),archive);validateArchive(archive,id);
    bool healthy=false;try {const auto current=read(id);(void)references(directory(id),current.definition);healthy=true;}catch(const std::exception&) {}
    if(healthy) (void)this->backup(id); // Keep a recoverable copy of the current complete world before replacement.
    const auto live=directory_/id,stage=directory_/(id+".restore-stage"),old=directory_/(id+".restore-old");
    removeTree(directory_,stage);std::filesystem::create_directories(stage);copyTree(archive,stage);
    {std::ifstream input(stage/"world.json");Json json;input>>json;if(!input || json.at("id")!=id) throw std::runtime_error("Invalid backed-up world metadata");(void)references(stage,WorldDefinition::parse(json.at("definition")));}
    atomicWrite(directory_/("restore-"+id+".json"),Json{{"version",1},{"id",id}}.dump());
    if(std::filesystem::exists(live)) {regularTree(live);renameDirectory(live,old);}
    try {renameDirectory(stage,live);} catch(...) {recover(id);throw;}
    recover(id);
}
size_t WorldStorage::collect(const std::string& id) const {
    IoExecutor::instance().submit([]{}).get();
    const auto record=read(id);const auto root=directory(id);const auto used=references(root,record.definition);size_t removed=0;
    if(std::filesystem::exists(root/"terrain")) regularTree(root/"terrain");
    if(std::filesystem::exists(root/"terrain")) for(const auto& entry:std::filesystem::directory_iterator(root/"terrain")) {
        regularTree(entry.path());const auto name=entry.path().filename().string();
        if(entry.is_regular_file() && (name.ends_with(".json") || name.ends_with(".index") || name.find(".pending-")!=std::string::npos) && !used.contains(name)) {std::filesystem::remove(entry.path());++removed;}
    }
    return removed;
}
}
