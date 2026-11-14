#include "tools/EquipmentAuthoring.h"
#include "assets/AssetId.h"
#include "assets/StudioPackage.h"
#include "assets/VirtualFileSystem.h"
#include "core/AtomicFile.h"
#include "mods/Manifest.h"
#include "scripting/ScriptCheck.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <set>
#include <windows.h>

namespace game {
void publishEquipment(const std::filesystem::path& mods,const AuthoredEquipment& equipment) {
    using Json=nlohmann::json;
    if(!validNamespace(equipment.mod) || equipment.mod=="core" || equipment.mod.size()>48 || equipment.name.empty() || equipment.name.size()>120)
        throw std::runtime_error("Informe um ID de mod válido e um nome de até 120 bytes");
    const auto destination=mods/equipment.mod;
    if(std::filesystem::exists(destination)) throw std::runtime_error("Já existe um pacote com esse ID");
    const auto discovery=discoverMods(mods);
    std::vector<std::string> dependencies{"foundation"};
    for(const auto& mod:discovery.mods) if(mod.id==equipment.mod) throw std::runtime_error("Já existe um mod com esse ID");
    const bool armor=!equipment.armorSlot.empty();
    const auto numeric=[](double value,double min,double max) {
        if(!std::isfinite(value) || value<min || value>max) throw std::runtime_error("Atributo fora dos limites");
        return Json(value).dump();
    };
    const auto scale=numeric(equipment.scale,.001,100);
    if(armor && equipment.armorSlot!="head" && equipment.armorSlot!="chest" && equipment.armorSlot!="legs" && equipment.armorSlot!="feet")
        throw std::runtime_error("Slot de armadura inválido");
    if(!armor && (equipment.ground.empty() || equipment.ground.size()>12 || equipment.air.size()>12))
        throw std::runtime_error("Defina de 1 a 12 golpes no chão e até 12 golpes aéreos");

    std::string model=equipment.model;
    if(equipment.studioManifest.empty()) {
        const auto owner=AssetId(model).nameSpace();
        bool available=false;
        for(const auto& mod:discovery.mods) if(mod.id==owner) {
            std::ifstream input(mod.root/"assets.json",std::ios::binary);
            if(!input) continue;
            const auto registry=Json::parse(input);
            available=!armor && registry.contains("models") && registry.at("models").contains(model);
            if(registry.contains("studio")) for(const auto& entry:registry.at("studio")) {
                const auto source=VirtualFileSystem::contained(mod.root,entry.at("source").get<std::string>());
                std::ifstream manifestFile(source,std::ios::binary);
                const auto manifest=Json::parse(manifestFile);
                for(const auto& candidate:manifest.at("models")) {
                    if(candidate.at("id")!=model) continue;
                    if(armor && (candidate.at("type")!="armor" || candidate.at("bindings").empty())) continue;
                    if(armor) {
                        const auto package=loadStudioPackage(source,std::filesystem::temp_directory_path()/"shardbound-studio-validation",owner);
                        if(!package->events.at("clips").empty() || !package->player.at("clips").empty() || package->player.at("context").is_null())
                            throw std::runtime_error("A armadura precisa de encaixes do Studio e deve estar sem clipes próprios");
                    }
                    available=true;
                }
            }
        }
        if(!available) throw std::runtime_error(armor?"Escolha um modelo de armadura com encaixes do Studio":"Modelo não encontrado nos mods ativos");
        if(owner!="foundation") dependencies.push_back(owner);
    }

    static std::atomic<unsigned> serial{};
    const auto stage=mods/(".editor-equipment-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++serial));
    if(!std::filesystem::create_directory(stage)) throw std::runtime_error("Não foi possível preparar o pacote de equipamento");
    try {
        if(!equipment.studioManifest.empty()) {
            const auto root=equipment.studioManifest.parent_path();
            auto studio=loadStudioPackage(equipment.studioManifest,stage/"cache");
            if(armor && (!studio->events.at("clips").empty() || !studio->player.at("clips").empty()))
                throw std::runtime_error("A armadura deve ser exportada sem clipes");
            if(studio->player.at("context").is_null()) throw std::runtime_error("O modelo precisa do jogador de referência do Studio");
            std::ifstream original(equipment.studioManifest,std::ios::binary);
            const auto manifest=Json::parse(original);
            const auto project=manifest.at("project").get<std::string>();
            if(AssetId(project).nameSpace()!=equipment.mod) throw std::runtime_error("O projeto exportado precisa usar o ID do novo mod como namespace");
            const auto& models=manifest.at("models");
            const auto selected=std::find_if(models.begin(),models.end(),[&](const auto& asset) {
                const auto type=asset.at("type").template get<std::string>();
                return armor?type=="armor":type=="melee" || type=="ranged";
            });
            if(selected==models.end()) throw std::runtime_error("A exportação não contém um modelo do tipo selecionado");
            if(armor && selected->at("bindings").empty()) throw std::runtime_error("Vincule a armadura aos grupos do jogador no Studio");
            model=selected->at("id").get<std::string>();
            if(!armor) {
                if(!studio->clip("idle")) throw std::runtime_error("A arma exportada precisa do clipe idle");
                for(const auto* sequence:{&equipment.ground,&equipment.air}) for(const auto& step:*sequence) {
                    const auto clip=step.clip.empty()?"attack":step.clip;
                    if(!studio->clip(clip)) throw std::runtime_error("Clipe ausente na exportação: "+clip);
                }
            }
            std::set<std::string> files{equipment.studioManifest.filename().string()};
            for(const auto* field:{"effects","events","playerAnimations"}) files.insert(manifest.at(field));
            for(const auto& asset:manifest.at("models")) files.insert(asset.at("file"));
            for(const auto& sound:studio->events.at("sounds")) files.insert(sound.at("file"));
            for(const auto& effect:studio->effects.at("definitions")) {
                const auto texture=effect.at("texture").get<std::string>();if(!texture.empty()) files.insert(texture);
            }
            for(const auto& path:files) {
                for(const auto& part:std::filesystem::path(path)) if(part==L"..") throw std::runtime_error("Caminho relativo inválido na exportação do Studio");
                const auto source=VirtualFileSystem::contained(root,path);
                const auto dest=stage/"studio"/std::filesystem::path(path).lexically_normal();
                std::filesystem::create_directories(dest.parent_path());
                std::filesystem::copy_file(source,dest);
            }
            atomicWrite(stage/"assets.json",Json{{"studio",{{project,{{"source","studio/"+equipment.studioManifest.filename().string()}}}}}}.dump(2));
        }

        std::string script;
        const auto quote=[](const std::string& text) {
            for(unsigned char c:text) if(c<32) throw std::runtime_error("Texto de equipamento contém caractere de controle");
            return Json(text).dump();
        };
        std::set<std::string> ids;
        const auto sequence=[&](const std::vector<AuthoredAttack>& attacks) {
            std::string list="[";
            for(const auto& attack:attacks) {
                if(!validNamespace(attack.id) || !ids.insert(attack.id).second || attack.id.find('-')!=std::string::npos ||
                    attack.id.empty() || attack.id.front()<'a' || attack.id.front()>'z') throw std::runtime_error("ID de golpe inválido ou repetido");
                if(attack.shape!="arc" && attack.shape!="circle" && attack.shape!="box") throw std::runtime_error("Formato de golpe inválido");
                if(list.size()>1) list+=", ";
                list+=quote(equipment.mod+":"+attack.id);
                script+="attack "+attack.id+":\n    .damage = "+numeric(attack.damage,0,1e6)+
                    "\n    .reach = "+numeric(attack.reach,.1,30)+"\n    .width = "+numeric(attack.width,.1,30)+
                    "\n    .height = "+numeric(attack.height,.1,30)+
                    "\n    .windup = "+numeric(attack.windup,.01,20)+"\n    .recovery = "+numeric(attack.recovery,.01,20)+
                    "\n    .shape = "+quote(attack.shape)+"\n    .clip = "+quote(attack.clip.empty()?"attack":attack.clip)+"\n";
            }
            return list+"]";
        };
        std::string ground,air;
        if(!armor) {script="@Attacks\n";ground=sequence(equipment.ground);air=sequence(equipment.air);}
        script+="\n@Items\n";
        script+=armor?"armor Equipment:\n":"weapon Equipment:\n";
        script+="    .name = "+quote(equipment.name)+"\n    .model = "+quote(model)+
            "\n    .scale = vec3("+scale+", "+scale+", "+scale+")\n    .rarity = \"Comum\"\n    .editor_grant = "+
            (equipment.grantOnStart?"true\n":"false\n");
        if(armor) script+="    .armor_slot = "+quote(equipment.armorSlot)+"\n    .protection = "+numeric(equipment.protection,0,1e6)+
            "\n    .movement_bonus = "+numeric(equipment.movementBonus,-100,100)+
            "\n    .health_bonus = "+numeric(equipment.healthBonus,-1e6,1e6)+"\n";
        else {
            const auto& base=equipment.ground.front();
            script+="    .damage = "+numeric(base.damage,0,1e6)+"\n    .reach = "+numeric(base.reach,.1,30)+
                "\n    .windup = "+numeric(base.windup,.01,20)+"\n    .recovery = "+numeric(base.recovery,.01,20)+
                "\n    .ground_attacks = "+ground+"\n    .air_attacks = "+air+"\n";
        }
        atomicWrite(stage/"main.sl",script);
        const auto checked=script::checkScripts(stage,"main.sl");
        if(!checked.diagnostics.empty()) throw DiagnosticError(checked.diagnostics.front());
        atomicWrite(stage/"mod.json",Json{{"id",equipment.mod},{"name",equipment.name},{"version","1.0.0"},
            {"api","0.1"},{"entry","main.sl"},{"dependencies",dependencies}}.dump(2));
        renameDirectory(stage,destination);
    } catch(...) {
        const auto resolved=std::filesystem::weakly_canonical(stage);
        if(resolved.parent_path()==std::filesystem::canonical(mods) && resolved.filename().wstring().starts_with(L".editor-equipment-")) {
            std::error_code ignored;std::filesystem::remove_all(resolved,ignored);
        }
        throw;
    }
}
}
