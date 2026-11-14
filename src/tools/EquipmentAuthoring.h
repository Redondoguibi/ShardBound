#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace game {
struct AuthoredAttack {
    std::string id,clip,shape="arc";
    double damage=15,reach=2.4,width=2,windup=.16,recovery=.34;
    double height=2;
};
struct AuthoredEquipment {
    std::string mod,name,model,armorSlot;
    std::filesystem::path studioManifest;
    double scale=1,protection=0;
    double movementBonus=0,healthBonus=0;
    bool grantOnStart=false;
    std::vector<AuthoredAttack> ground,air;
    std::string editorProject;
};
void publishEquipment(const std::filesystem::path& mods,const AuthoredEquipment& equipment);
AuthoredEquipment readEditorEquipment(const std::filesystem::path& root);
}
