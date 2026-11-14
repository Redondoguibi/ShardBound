#include "tools/EditorTimeline.h"
#include "core/AtomicFile.h"
#include "core/PathText.h"
#include "world/WorldDefinition.h"
#include <Windows.h>
#include <objbase.h>
#include <fstream>
#include <limits>
#include <map>

namespace game {
namespace {
using Json=nlohmann::json;
constexpr size_t timelineLimit=128*1024*1024;
constexpr size_t mapMemoryLimit=512*1024*1024;
constexpr std::string_view prefix=".shardbound-timeline-";

void require(bool ok) {
    if(!ok) throw std::runtime_error("Histórico de edições inválido");
}

int integer(const Json& value) {
    require(value.is_number_integer());
    const auto number=value.get<double>();
    require(number>=std::numeric_limits<int>::min() && number<=std::numeric_limits<int>::max());
    return int(number);
}

Json point(FiniteMap::Spawn p) {return Json::array({p.x,p.z});}
FiniteMap::Spawn readPoint(const Json& value) {
    require(value.is_array() && value.size()==2);
    return {integer(value[0]),integer(value[1])};
}

Json optionalPoint(const std::optional<FiniteMap::Spawn>& p) {return p?point(*p):Json(nullptr);}
std::optional<FiniteMap::Spawn> readOptionalPoint(const Json& value) {
    if(value.is_null()) return std::nullopt;
    return readPoint(value);
}

WorldDefinition readCatalog(const std::filesystem::path& content) {
    require(content.is_absolute() && std::filesystem::is_directory(content));
    auto discovered=discoverMods(content);
    for(const auto& diagnostic:discovered.diagnostics)
        if(diagnostic.code!="MOD106") throw DiagnosticError(diagnostic);
    return WorldDefinition::load(discovered.mods);
}

using SnapshotIds=std::map<const FiniteMap*,size_t>;
void accountMap(const FiniteMap& map,size_t& bytes) {
    const auto bounds=map.bounds();
    bytes+=size_t(bounds.width)*bounds.depth*FiniteMap::height*sizeof(std::uint16_t);
    if(bytes>mapMemoryLimit) throw std::runtime_error("Os mapas da linha do tempo excedem 512 MiB em memória");
    for(const auto& room:map.interiors()) accountMap(*room.map,bytes);
}

Json encode(const EditorEdit& edit,SnapshotIds& ids,Json& snapshots,size_t& bytes) {
    const auto length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,edit.label.data(),int(edit.label.size()),nullptr,0,nullptr,nullptr);
    require(length>0 && length<=4096);
    std::string label(size_t(length),'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,edit.label.data(),int(edit.label.size()),label.data(),length,nullptr,nullptr);
    Json entry={{"label",std::move(label)}};
    if(!edit.cells.empty()) {
        auto& cells=entry["cells"]=Json::array();
        for(const auto& c:edit.cells) cells.push_back({c.x,c.y,c.z,c.before,c.after});
    }
    if(edit.spawn) entry["spawn"]={point(edit.spawn->first),point(edit.spawn->second)};
    if(edit.exit) entry["exit"]={optionalPoint(edit.exit->first),optionalPoint(edit.exit->second)};
    if(edit.prop) {
        const auto& p=*edit.prop;
        entry["prop"]={p.model,p.x,p.y,p.z,p.yaw,p.scale};
    }
    if(edit.interior) {
        const auto& room=*edit.interior;
        accountMap(*room.map,bytes);
        entry["interior"]={{"entrance",point(room.entrance)},{"return",point(room.returnPoint)},{"map",room.map->json().dump()}};
    }
    if(edit.content) entry["content"]={pathText(edit.content->first),pathText(edit.content->second)};
    if(edit.revision) {
        const auto save=[&](const std::shared_ptr<const FiniteMap>& map) {
            const auto [it,inserted]=ids.try_emplace(map.get(),snapshots.size());
            if(inserted) {
                accountMap(*map,bytes);
                snapshots.push_back(map->json().dump());
            }
            return it->second;
        };
        entry["revision"]={save(edit.revision->first),save(edit.revision->second)};
    }
    return entry;
}

struct Snapshot {
    std::filesystem::path content;
    std::shared_ptr<const FiniteMap> map;
};

EditorEdit decode(const Json& entry,bool forward,std::filesystem::path& activeContent,
    const WorldDefinition*& catalog,std::unique_ptr<WorldDefinition>& loadedCatalog,
    const Json& snapshots,std::vector<Snapshot>& decoded,size_t& bytes) {
    EditorEdit edit;
    const auto& label=entry.at("label").get_ref<const std::string&>();
    require(!label.empty() && label.find('\0')==std::string::npos && label.size()<=4096);
    const auto length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,label.data(),int(label.size()),nullptr,0);
    require(length>0);
    edit.label.resize(size_t(length));
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,label.data(),int(label.size()),edit.label.data(),length);
    if(entry.contains("cells")) {
        const auto& cells=entry.at("cells");
        require(cells.is_array() && !cells.empty() && cells.size()<=editorCellEditLimit);
        for(const auto& c:cells) {
            require(c.is_array() && c.size()==5);
            auto& cell=edit.cells.emplace_back(integer(c[0]),integer(c[1]),integer(c[2]),c[3].get<std::string>(),c[4].get<std::string>());
            require((cell.before.empty() || catalog->blocks.contains(cell.before)) &&
                (cell.after.empty() || catalog->blocks.contains(cell.after)));
        }
    }
    if(entry.contains("spawn")) {
        const auto& p=entry.at("spawn");
        require(p.is_array() && p.size()==2);
        edit.spawn=std::pair{readPoint(p[0]),readPoint(p[1])};
    }
    if(entry.contains("exit")) {
        const auto& p=entry.at("exit");
        require(p.is_array() && p.size()==2);
        edit.exit=std::pair{readOptionalPoint(p[0]),readOptionalPoint(p[1])};
    }
    if(entry.contains("prop")) {
        const auto& p=entry.at("prop");
        require(p.is_array() && p.size()==6 && p[4].is_number() && p[5].is_number());
        edit.prop=FiniteMap::Prop{p[0].get<std::string>(),integer(p[1]),integer(p[2]),integer(p[3]),p[4].get<float>(),p[5].get<float>()};
    }
    if(entry.contains("interior")) {
        const auto& room=entry.at("interior");
        auto map=std::make_shared<FiniteMap>(FiniteMap::parseDraft(Json::parse(room.at("map").get_ref<const std::string&>()),*catalog));
        accountMap(*map,bytes);
        require(map->exit().has_value() && map->interiors().empty());
        edit.interior=FiniteMap::Interior{readPoint(room.at("entrance")),readPoint(room.at("return")),*map->exit(),std::move(map)};
    }
    if(entry.contains("content")) {
        const auto& paths=entry.at("content");
        require(paths.is_array() && paths.size()==2);
        const auto path=[](const Json& value) {
            const auto& name=value.get_ref<const std::string&>();
            require(name.find('\0')==std::string::npos);
            return std::filesystem::path(std::u8string(name.begin(),name.end()));
        };
        edit.content=std::pair{path(paths[0]),path(paths[1])};
        require(activeContent==(forward?edit.content->first:edit.content->second));
    }
    const int mapOperations=int(!edit.cells.empty())+int(bool(edit.spawn))+int(bool(edit.exit))+
        int(bool(edit.prop))+int(bool(edit.interior))+int(entry.contains("revision"));
    require(mapOperations<=1 && (mapOperations!=0 || edit.content));
    require(!edit.content || mapOperations==0 || entry.contains("revision"));

    const auto readSnapshot=[&](const Json& index) {
        const auto id=integer(index);
        require(id>=0 && size_t(id)<decoded.size());
        auto& snapshot=decoded[size_t(id)];
        if(!snapshot.map) {
            snapshot.map=std::make_shared<FiniteMap>(FiniteMap::parseDraft(
                Json::parse(snapshots.at(id).get_ref<const std::string&>()),*catalog));
            accountMap(*snapshot.map,bytes);
            snapshot.content=activeContent;
        } else require(snapshot.content==activeContent);
        return snapshot.map;
    };
    const auto revision=entry.find("revision");
    if(revision!=entry.end()) require(revision->is_array() && revision->size()==2);
    const auto from=revision==entry.end()?nullptr:readSnapshot((*revision)[forward?0:1]);
    if(edit.content) {
        activeContent=forward?edit.content->second:edit.content->first;
        loadedCatalog=std::make_unique<WorldDefinition>(readCatalog(activeContent));
        catalog=loadedCatalog.get();
    }
    if(revision!=entry.end()) {
        auto to=readSnapshot((*revision)[forward?1:0]);
        edit.revision=forward?std::pair{from,to}:std::pair{to,from};
    }
    return edit;
}
}

void applyEditorMapEdit(FiniteMap& map,const WorldDefinition& catalog,const EditorEdit& edit,bool forward) {
    if(edit.revision) map=*(forward?edit.revision->second:edit.revision->first);
    if(edit.prop) {
        if(forward) map.addProp(*edit.prop);
        else {
            require(!map.props().empty());
            const auto& last=map.props().back();
            const auto& prop=*edit.prop;
            require(last.model==prop.model && last.x==prop.x && last.y==prop.y && last.z==prop.z &&
                last.yaw==prop.yaw && last.scale==prop.scale);
            map.removeProp();
        }
    }
    if(edit.interior) {
        if(forward) map.addInterior(*edit.interior);
        else {
            require(!map.interiors().empty());
            const auto& last=map.interiors().back();
            require(last.entrance==edit.interior->entrance && last.returnPoint==edit.interior->returnPoint && last.exit==edit.interior->exit);
            map.removeInterior();
        }
    }
    if(edit.exit) map.setExit(forward?edit.exit->second:edit.exit->first);
    if(edit.spawn) {
        const auto p=forward?edit.spawn->second:edit.spawn->first;
        map.setSpawn(p.x,p.z);
    }
    for(const auto& c:edit.cells) {
        require(map.block(c.x,c.y,c.z)==(forward?c.before:c.after));
        map.setBlock(c.x,c.y,c.z,forward?c.after:c.before,catalog);
    }
}

std::string writeEditorTimeline(const std::filesystem::path& project,const std::vector<EditorEdit>& edits,size_t cursor) {
    if(edits.empty()) return {};
    SnapshotIds ids;
    auto snapshots=Json::array(),entries=Json::array();
    size_t bytes=0;
    for(const auto& edit:edits) entries.push_back(encode(edit,ids,snapshots,bytes));
    const auto data=Json{{"version",1},{"cursor",cursor},{"edits",std::move(entries)},{"maps",std::move(snapshots)}}.dump();
    if(data.size()>timelineLimit) throw std::runtime_error("A linha do tempo excede 128 MiB");
    GUID generation{};
    wchar_t id[39]{};
    if(FAILED(CoCreateGuid(&generation)) || !StringFromGUID2(generation,id,39))
        throw std::runtime_error("Não foi possível criar o arquivo da linha do tempo");
    const auto file=std::string(prefix)+pathText(id).substr(1,36)+".json";
    // Publish the history first so both the current project and its backup always point to complete files.
    atomicWrite(project.parent_path()/file,data);
    return file;
}

EditorTimeline readEditorTimeline(const std::filesystem::path& project,const std::string& file,
    const FiniteMap& map,const std::filesystem::path& content,const WorldDefinition& catalog) {
    require(file.size()==prefix.size()+36+5 && file.starts_with(prefix) && file.ends_with(".json") &&
        file.substr(prefix.size(),36).find_first_not_of("0123456789ABCDEFabcdef-")==std::string::npos);
    const auto path=project.parent_path()/file;
    require(std::filesystem::file_size(path)<=timelineLimit);
    std::ifstream input(path,std::ios::binary);
    const auto document=Json::parse(input);
    require(document.at("version")==1);
    const auto& entries=document.at("edits");
    const auto& snapshots=document.at("maps");
    const auto cursor=integer(document.at("cursor"));
    require(entries.is_array() && snapshots.is_array() && cursor>=0 && size_t(cursor)<=entries.size());
    EditorTimeline result{std::vector<EditorEdit>(entries.size()),size_t(cursor)};
    std::vector<Snapshot> decoded(snapshots.size());
    size_t bytes=0;
    for(const bool forward:{false,true}) {
        auto current=map;
        auto activeContent=content;
        const WorldDefinition* activeCatalog=&catalog;
        std::unique_ptr<WorldDefinition> loadedCatalog;
        const auto restore=[&](size_t index) {
            auto edit=decode(entries[index],forward,activeContent,activeCatalog,loadedCatalog,snapshots,decoded,bytes);
            if(edit.revision) {
                const auto& from=forward?edit.revision->first:edit.revision->second;
                require(current.json()==from->json());
            }
            applyEditorMapEdit(current,*activeCatalog,edit,forward);
            result.edits[index]=std::move(edit);
        };
        if(forward) for(size_t i=result.cursor;i<entries.size();++i) restore(i);
        else for(size_t i=result.cursor;i>0;--i) restore(i-1);
    }
    return result;
}
}
