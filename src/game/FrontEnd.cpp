#include "game/FrontEnd.h"
#include <algorithm>
#include <cmath>
#include "core/IoExecutor.h"
#include "assets/AssetId.h"
#include "persistence/SaveGame.h"
#include "world/FiniteMap.h"
namespace game {
namespace {
std::string versionText(Version version) {return std::to_string(version.parts[0])+"."+std::to_string(version.parts[1])+"."+std::to_string(version.parts[2]);}
std::vector<std::string> actions(const InputSystem& input) {std::vector<std::string> result;for(const auto& [id,b]:input.bindings()) {(void)b;if(id!="Quit") result.push_back(id);}return result;}
std::string bindingText(const std::vector<InputBinding>& bindings) {std::string result;for(const auto& b:bindings) {if(b.key!=Key::Unknown) result+=keyName(b.key)+" ";if(b.mouse!=MouseButton::Unknown) result+=b.mouse==MouseButton::Left?"Mouse esquerdo ":b.mouse==MouseButton::Right?"Mouse direito ":"Mouse central ";}return result.empty()?"Controle":result;}
std::string actionLabel(const std::string& id) {const std::map<std::string,std::string> labels={{"MoveForward","Andar para frente"},{"MoveBackward","Andar para tras"},{"MoveLeft","Andar para a esquerda"},{"MoveRight","Andar para a direita"},{"Jump","Pular"},{"Attack","Atacar"},{"Interact","Interagir"},{"Inventory","Inventario"},{"Map","Mapa"},{"Equip","Equipar"},{"Dodge","Esquivar"},{"Save","Salvar"},{"Load","Carregar"},{"Travel1","Viagem 1"},{"Travel2","Viagem 2"},{"Travel3","Viagem 3"}};const auto i=labels.find(id);return i==labels.end()?id:i->second;}
const char* screenTitle(FrontEnd::Screen screen) {
    using Screen=FrontEnd::Screen;
    switch(screen) {case Screen::Title:return "Um mundo para explorar";case Screen::Worlds:return "Seus mundos";case Screen::Create:return "Criar mundo";case Screen::Settings:return "Configuracoes";case Screen::Backups:return "Restaurar backup";case Screen::Controls:return "Controles e interface";case Screen::Audio:return "Canais de audio";case Screen::Mods:return "Mods e perfis";case Screen::ModIssues:return "Diagnosticos de mods";case Screen::ProfileName:return "Nome do perfil";default:return "Pausa";}
}
}
FrontEnd::FrontEnd(Renderer& renderer,WindowSystem& window,InputSystem& input,std::filesystem::path user,WorldDefinition definition,std::map<std::string,std::string> mods)
    :renderer_(renderer),window_(window),input_(input),storage_(user/"worlds"),settingsFile_(user/"settings.json"),definition_(std::move(definition)),mods_(std::move(mods)) {
    try { settings_=GameSettings::load(settingsFile_); } catch(const std::exception& e) { message_=std::string("Configuracoes invalidas: ")+e.what(); }
}
void FrontEnd::configureMods(std::vector<ModManifest> catalog,std::set<std::string> available,std::vector<Diagnostic> diagnostics) {
    modDiagnostics_=std::move(diagnostics);const auto file=settingsFile_.parent_path()/"mod-profiles.json";
    try {profiles_=std::make_unique<ModProfiles>(file,catalog,available);}catch(const std::exception& error) {profiles_=std::make_unique<ModProfiles>(file,catalog,available,true);message_=std::string("Perfis invalidos; usando padrao: ")+error.what();}
    try {refreshProfile();}catch(const std::exception& error) {message_=error.what();}
}
WorldRecord FrontEnd::createRecord(std::string name,std::string seed) {
    if(profiles_) {const auto errors=profiles_->issues();if(!errors.empty()) throw std::runtime_error(errors.front());}
    if(!definition_.maps.empty()) return storage_.create(std::move(name),"shardbound",definition_.maps.begin()->first,definition_,mods_);
    if(definition_.generators.empty()) throw std::runtime_error("Ative um mod com gerador de mundo");
    auto generator=definition_.generators.begin();std::advance(generator,generatorIndex_);return storage_.create(std::move(name),std::move(seed),generator->first,definition_,mods_);
}
WorldRecord FrontEnd::createMapTestRecord(const std::filesystem::path& source) {
    if(profiles_) {const auto errors=profiles_->issues();if(!errors.empty()) throw std::runtime_error(errors.front());}
    auto definition=definition_;
    if(definition.maps.empty()) throw std::runtime_error("Ative o mod do mapa oficial para testar");
    auto& [id,map]=*definition.maps.begin();
    map=std::make_shared<FiniteMap>(FiniteMap::read(source,definition));
    const auto spawn=map->spawn();
    if(TerrainGenerator(definition,id,"shardbound").height(spawn.x,spawn.z)<1)
        throw std::runtime_error("O mapa precisa de chão no ponto inicial");
    return storage_.create("Teste do editor","shardbound",id,definition,mods_);
}
void FrontEnd::refreshProfile() {
    definition_={};mods_.clear();generatorIndex_=0;
    const auto errors=profiles_->issues();if(!errors.empty()) {definition_={};mods_.clear();message_=errors.front();return;}
    const auto selected=profiles_->selected();for(const auto& mod:selected) mods_[mod.id]=versionText(mod.version);
    // Selection is independent of generation: existing worlds keep their frozen definition.
    if(std::none_of(selected.begin(),selected.end(),[](const auto& mod){return std::filesystem::exists(mod.root/"world.json");})) {message_="Ative um mod de geracao para criar mundos.";return;}
    definition_=WorldDefinition::load(selected);
}
std::vector<std::string> FrontEnd::profileIds() const {std::vector<std::string> ids;if(!profiles_) return ids;for(const auto& m:profiles_->catalog()) ids.push_back(m.id);for(const auto& id:profiles_->enabled()) if(std::find(ids.begin(),ids.end(),id)==ids.end()) ids.push_back(id);return ids;}
void FrontEnd::changedSettings(const GameSettings& old) {
    try {if(applySettings) applySettings(settings_);settings_.save(settingsFile_);message_="Configuracoes aplicadas e salvas.";}
    catch(...) {settings_=old;if(applySettings) applySettings(old);throw;}
}
void FrontEnd::navigate(Screen screen) {
    screen_=screen;message_.clear();window_.textInput(screen==Screen::Create || screen==Screen::ProfileName);
    if(screen==Screen::Worlds) { std::vector<std::string> errors;worlds_=storage_.list(&errors);page_=0;if(!errors.empty()) message_="Um mundo nao pode ser lido: "+errors.front(); }
}
void FrontEnd::error(std::string message) {
    if(playing()) navigate(Screen::Pause);
    saveError_=std::move(message);
    // Saving fails after the menu update in Application: draw the warning this frame.
    // Do not reuse the attack click that may still be set from the last menu frame.
    click_=false;draw();
}
void FrontEnd::event(const Event& e,bool uiCaptured) {
    if(maintenance_.valid()) return;
    if(screen_==Screen::Controls && !rebinding_.empty()) {
        if(e.type==EventType::FocusLost) {rebinding_.clear();return;}
        if(e.type==EventType::KeyDown && e.key==Key::Escape) {rebinding_.clear();bindingConsumed_=true;return;}
        if((e.type==EventType::KeyDown && e.key!=Key::Unknown) || (e.type==EventType::MouseDown && e.mouse!=MouseButton::Unknown)) {
            if(e.key==Key::PageUp || e.key==Key::PageDown || e.key==Key::Slash || e.key==Key::F3) {message_="Tecla reservada para ferramentas de desenvolvimento.";return;}
            const auto old=settings_;const auto action=rebinding_;
            if(e.type==EventType::KeyDown && e.key==Key::Delete) settings_.bindings.erase(action);
            else {
                InputBinding binding;binding.key=e.key;binding.mouse=e.mouse;std::vector<InputBinding> remapped{binding};
                const auto bindings=input_.bindings();
                if(const auto found=bindings.find(action);found!=bindings.end()) for(const auto& previous:found->second)
                    if(previous.pad!=PadButton::Unknown && std::none_of(remapped.begin(),remapped.end(),[&](const auto& value){return value.pad==previous.pad;})) remapped.push_back({Key::Unknown,MouseButton::Unknown,previous.pad});
                settings_.bindings[action]=std::move(remapped);
            }
            rebinding_.clear();bindingConsumed_=true;try {changedSettings(old);}catch(const std::exception& error) {message_=error.what();}return;
        }
        return;
    }
    if(uiCaptured && playing()) return;
    if(e.type==EventType::KeyDown && e.key==Key::Escape) {
        if(screen_==Screen::Playing) navigate(Screen::Pause);
        else if(screen_==Screen::Pause) { saveError_.clear();navigate(Screen::Playing); }
        else if(screen_==Screen::Settings) navigate(settingsReturn_);
        else if(screen_==Screen::Controls || screen_==Screen::Audio) navigate(Screen::Settings);
        else if(screen_==Screen::Mods) navigate(profileWorld_.empty()?Screen::Title:Screen::Worlds);
        else if(screen_==Screen::ModIssues) navigate(Screen::Mods);
        else if(screen_==Screen::ProfileName) navigate(Screen::Mods);
        else if(screen_==Screen::Create) navigate(Screen::Worlds);
        else if(screen_==Screen::Backups) navigate(Screen::Worlds);
        else if(screen_==Screen::Worlds) navigate(Screen::Title);
    }
    if(screen_==Screen::ProfileName) {
        if(e.type==EventType::TextInput && profileName_.size()+e.text.size()<=64 && std::none_of(e.text.begin(),e.text.end(),[](unsigned char c){return c<32 || c==127;})) profileName_+=e.text;
        if(e.type==EventType::KeyDown && e.key==Key::Backspace && !profileName_.empty()) {size_t i=profileName_.size()-1;while(i && (static_cast<unsigned char>(profileName_[i])&0xc0)==0x80) --i;profileName_.erase(i);}
        if(e.type==EventType::KeyDown && e.key==Key::Enter) {try {action(79);}catch(const std::exception& error) {message_=error.what();}}return;
    }
    if(screen_!=Screen::Create) return;
    auto& value=focus_==0 ? name_ : seed_;
    if(e.type==EventType::TextInput) {
        const size_t limit=focus_==0 ? 96 : 128;
        for(unsigned char c:e.text) if(c<32 || c==127) return;
        if(value.size()+e.text.size()<=limit) value+=e.text;
    }
    if(e.type==EventType::KeyDown) {
        if(e.key==Key::Tab && definition_.maps.empty()) focus_=1-focus_;
        if(e.key==Key::Backspace && !value.empty()) { size_t i=value.size()-1;while(i && (static_cast<unsigned char>(value[i])&0xc0)==0x80) --i;value.erase(i); }
        if(e.key==Key::Enter) { try { action(20); } catch(const std::exception& error) { message_=error.what(); } }
    }
}
bool FrontEnd::hit(int x,int y,int w,int h) const {
    const float scale=std::min(input_.width()/1280.f,input_.height()/720.f)*input_.uiScale();
    const float px=(input_.mouseX()-(input_.width()-1280*scale)/2)/scale,py=(input_.mouseY()-(input_.height()-720*scale)/2)/scale;
    return input_.hasPointer() && px>=x && py>=y && px<x+w && py<y+h;
}
void FrontEnd::text(int x,int y,int w,int size,std::string value,std::array<unsigned char,4> color) { CanvasCommand c;c.kind="text";c.x=x;c.y=y;c.w=w;c.h=size+14;c.size=size;c.text=std::move(value);c.align="left";c.color=color;commands_.push_back(std::move(c)); }
void FrontEnd::rectangle(int x,int y,int w,int h,std::array<unsigned char,4> color) { CanvasCommand c;c.kind="rect";c.x=x;c.y=y;c.w=w;c.h=h;c.color=color;commands_.push_back(std::move(c)); }
void FrontEnd::button(int id,int x,int y,int w,int h,const std::string& label) {
    const bool hovered=hit(x,y,w,h);rectangle(x,y,w,h,hovered ? std::array<unsigned char,4>{65,99,74,255} : std::array<unsigned char,4>{37,57,47,255});
    rectangle(x,y,4,h,{138,177,104,255});text(x+20,y+(h-32)/2,w-36,22,label);
    if(click_ && hovered && !maintenance_.valid()) { click_=false;action(id); }
}
void FrontEnd::open(const WorldRecord& record,bool resume) {
    if(!startWorld) throw std::runtime_error("World service unavailable");
    commands_.clear();rectangle(0,0,1280,720,{16,24,23,255});text(350,320,650,28,"Preparando "+record.name+"...");
    texture_=renderer_.createTexture(1280,720,canvasImage(commands_),false,false);renderer_.setCanvasOverlay(true);renderer_.render(Camera{},std::vector<RenderItem>{},texture_);renderer_.present();
    try {startWorld(record,resume);}catch(const std::exception& error) {modDiagnostics_.push_back({"MODPROFILE",error.what()});throw;}active_=true;navigate(Screen::Playing);
}
void FrontEnd::action(int id) {
    if(id==4 || (id>=400 && id<404)) {if(active_) throw std::runtime_error("Feche o mundo antes de alterar mods");profileWorld_=id==4?std::string{}:worlds_.at(page_*4+size_t(id-400)).id;modsPage_=0;navigate(Screen::Mods);}
    else if(id==80) {controlsPage_=0;navigate(Screen::Controls);}
    else if(id==81) navigate(Screen::Audio);
    else if(id==83) {rebinding_.clear();navigate(Screen::Settings);}
    else if(id==84 && controlsPage_) --controlsPage_;
    else if(id==85 && (controlsPage_+1)*5<actions(input_).size()) ++controlsPage_;
    else if(id>=500 && id<505) rebinding_=actions(input_).at(controlsPage_*5+size_t(id-500));
    else if(id==90 || id==91 || (id>=600 && id<604)) {
        if(active_ || !profiles_) throw std::runtime_error("Gerenciamento de mods indisponivel durante uma partida");
        if(id==90) profiles_->next();else if(id==91) profiles_->duplicate();else profiles_->toggle(profileIds().at(modsPage_*4+size_t(id-600)));
        refreshProfile();
    }
    else if(id==92 && modsPage_) --modsPage_;
    else if(id==93 && (modsPage_+1)*4<profileIds().size()) ++modsPage_;
    else if(id==95) {issuesPage_=0;navigate(Screen::ModIssues);}
    else if(id==96 && issuesPage_) --issuesPage_;
    else if(id==97 && (issuesPage_+1)*4<modDiagnostics_.size()+(profiles_?profiles_->issues().size():0)) ++issuesPage_;
    else if(id==98) navigate(Screen::Mods);
    else if(id==99 && profiles_) {profileName_=profiles_->active();navigate(Screen::ProfileName);}
    else if(id==79 && profiles_) {profiles_->rename(profileName_);navigate(Screen::Mods);}
    else if(id==78) {if(!exportPerformance) throw std::runtime_error("Diagnostico indisponivel");message_=exportPerformance();}
    else if(id==94) {
        if(profileWorld_.empty() || !profiles_ || active_) throw std::runtime_error("Selecione um mundo fechado");
        const auto selected=profiles_->selected();std::map<std::string,std::string> versions;for(const auto& mod:selected) versions[mod.id]=versionText(mod.version);auto record=storage_.read(profileWorld_);
        const auto require=[&](const std::string& resource) {if(!resource.empty() && AssetId(resource).nameSpace()!="core" && !versions.contains(AssetId(resource).nameSpace())) throw std::runtime_error("O terreno salvo requer o mod: "+AssetId(resource).nameSpace());};
        require(record.generator);for(const auto& [blockId,block]:record.definition.blocks) {(void)blockId;require(block.texture);require(block.shader);require(block.shaderTexture);}
        const auto checkpoint=storage_.directory(record.id)/"checkpoint.json";
        if(std::filesystem::exists(checkpoint)) {const auto saved=nlohmann::json::parse(encodeData(SaveGame::inspect(checkpoint)));for(const auto& m:selected) if(saved["mods"].contains(m.id) && !m.acceptsSave(saved["mods"][m.id],saved.value("mod_schemas",nlohmann::json::object()).value(m.id,1u))) throw std::runtime_error(m.id+": versao incompativel com o save");}
        record.mods=std::move(versions);record.schemas.clear();for(const auto& m:selected) record.schemas[m.id]=m.saveSchema;
        const auto storage=storage_;maintenance_=IoExecutor::instance().submit([storage,record]{const auto backup=storage.backup(record.id);storage.update(record);return "Perfil aplicado ao mundo. Backup: "+backup;});screen_=Screen::Worlds;message_="Aplicando perfil e criando backup...";
    }
    else if(id==77 || id==82 || id==86 || id==87 || id==88 || (id>=450 && id<456)) {
        const auto old=settings_;
        if(id==77) settings_.performance=!settings_.performance;
        if(id==82) settings_.bindings.clear();
        if(id==86) settings_.sensitivity=std::max(.1f,settings_.sensitivity-.1f);
        if(id==87) settings_.sensitivity=std::min(4.f,settings_.sensitivity+.1f);
        if(id==88) settings_.uiScale=settings_.uiScale<.8f?.875f:settings_.uiScale<.95f?1.f:.75f;
        if(id>=450 && id<456) {float* volumes[]={&settings_.effectsVolume,&settings_.musicVolume,&settings_.interfaceVolume};auto& value=*volumes[(id-450)/2];value=std::clamp(value+((id-450)%2?.1f:-.1f),0.f,1.f);}
        changedSettings(old);
    }
    else if(id>=300 && id<304) {
        const auto storage=storage_;const auto worldId=backupWorld_,backupId=backups_.at(page_*4+size_t(id-300));
        maintenance_=IoExecutor::instance().submit([storage,worldId,backupId]{storage.restoreBackup(worldId,backupId);return std::string("Backup restaurado.");});message_="Restauracao em andamento...";
    }
    else if(id>=200 && id<212) {
        if(active_) throw std::runtime_error("Feche o mundo antes de executar manutencao");
        const auto index=page_*4+size_t((id-200)/3);const auto worldId=worlds_.at(index).id;const int operation=(id-200)%3;
        if(operation==1) {backupWorld_=worldId;backups_=storage_.backups(worldId);std::reverse(backups_.begin(),backups_.end());page_=0;navigate(Screen::Backups);return;}
        const auto storage=storage_;maintenance_=IoExecutor::instance().submit([storage,worldId,operation] {
            if(operation==0) return std::string("Backup criado: ")+storage.backup(worldId);
            return std::string("Arquivos de terreno sem referencia removidos: ")+std::to_string(storage.collect(worldId));
        });message_="Manutencao em andamento...";
    }
    else if(id==1) navigate(Screen::Worlds);
    else if(id==2) { settingsReturn_=screen_;navigate(Screen::Settings); }
    else if(id==3) quit_=true;
    else if(id==10) { name_.clear();seed_.clear();focus_=0;navigate(Screen::Create); }
    else if(id==11) navigate(screen_==Screen::Backups || (screen_==Screen::Mods && !profileWorld_.empty())?Screen::Worlds:Screen::Title);
    else if(id==12 && page_>0) --page_;
    else if(id==13 && (page_+1)*4<(screen_==Screen::Backups?backups_.size():worlds_.size())) ++page_;
    else if(id>=100 && id<104) { auto record=worlds_.at(page_*4+size_t(id-100));open(record,std::filesystem::exists(storage_.directory(record.id)/"checkpoint.json")); }
    else if(id==20) {auto record=createRecord(name_,seed_);open(record,false);}
    else if(id==21) navigate(Screen::Worlds);
    else if(id==24 && definition_.maps.empty() && !definition_.generators.empty()) generatorIndex_=(generatorIndex_+1)%definition_.generators.size();
    else if(id==30) { saveError_.clear();navigate(Screen::Playing); }
    else if(id==31 || id==32) {
        try { if(saveWorld) saveWorld(); }
        catch(const std::exception& e) { saveError_=e.what();return; }
        saved();message_="Mundo salvo.";
        if(id==32) { if(closeWorld) closeWorld();active_=false;navigate(Screen::Title); }
    }
    else if(id==49) navigate(settingsReturn_);
    else if(id>=40 && id<=46) {
        auto old=settings_;
        if(id==40) settings_.vsync=!settings_.vsync;
        if(id==41) settings_.fullscreen=!settings_.fullscreen;
        if(id==42) settings_.shadows=!settings_.shadows;
        if(id==43) settings_.scale=settings_.scale>.9f ? .75f : settings_.scale>.6f ? .5f : 1.f;
        if(id==44) settings_.viewDistance=settings_.viewDistance==5 ? 2 : settings_.viewDistance+1;
        if(id==45) settings_.volume=std::max(0.f,settings_.volume-.1f);
        if(id==46) settings_.volume=std::min(1.f,settings_.volume+.1f);
        changedSettings(old);
    }
}
void FrontEnd::update() {
    if(maintenance_.valid() && maintenance_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {message_=maintenance_.get();if(screen_==Screen::Backups) {screen_=Screen::Worlds;page_=0;}worlds_=storage_.list();if(page_*4>=worlds_.size()) page_=0;}catch(const std::exception& e) {message_=e.what();}
    }
    if(playing()) return;
    click_=input_.pointerPressed() && rebinding_.empty() && !bindingConsumed_;bindingConsumed_=false;try { draw(); } catch(const std::exception& e) { message_=e.what();click_=false; }
}
void FrontEnd::draw() {
    commands_.clear();rectangle(0,0,1280,720,{15,23,22,250});rectangle(0,0,16,720,{122,155,88,255});
    text(100,65,1080,16,"S H A R D B O U N D",{159,191,131,255});
    text(100,110,1080,42,screen_==Screen::Mods && !profileWorld_.empty()?"Mods deste mundo":screenTitle(screen_));
    if(screen_==Screen::Title) { text(100,180,1000,20,"Explore mundos de blocos gerados pela seed. Conteudo criado por mods.");button(1,100,280,440,66,"Jogar");button(2,100,364,440,66,"Configuracoes");button(3,100,448,440,66,"Sair");button(4,600,280,480,66,"Mods e perfis"); }
    else if(screen_==Screen::Worlds) {
        for(size_t i=0;i<4 && page_*4+i<worlds_.size();++i) { const auto& w=worlds_[page_*4+i];button(100+int(i),100,205+int(i)*80,420,66,w.name+"  |  seed: "+w.seed);button(400+int(i),540,205+int(i)*80,130,66,"Mods");
            button(200+int(i)*3,690,205+int(i)*80,150,66,"Backup");button(201+int(i)*3,860,205+int(i)*80,150,66,"Restaurar");button(202+int(i)*3,1030,205+int(i)*80,150,66,"Limpar"); }
        if(worlds_.empty()) text(100,245,1080,24,"Nenhum mundo criado. Crie o primeiro abaixo.");
        button(10,100,550,360,56,"Criar mundo");button(11,480,550,200,56,"Voltar");button(12,900,550,130,56,"Anterior");button(13,1044,550,136,56,"Proximo");
    } else if(screen_==Screen::Backups) {
        for(size_t i=0;i<4 && page_*4+i<backups_.size();++i) button(300+int(i),100,205+int(i)*80,1080,66,"Restaurar: "+backups_[page_*4+i]);
        if(backups_.empty()) text(100,245,1080,24,"Nenhum backup disponivel para este mundo.");
        text(100,530,1080,16,"O mundo atual valido sera copiado antes da restauracao. Mais recentes primeiro.");
        button(11,100,570,240,50,"Voltar");button(12,900,570,130,50,"Anterior");button(13,1044,570,136,50,"Proximo");
    } else if(screen_==Screen::Create) {
        text(100,188,1080,20,"Nome do mundo");rectangle(100,224,1080,60,focus_==0 ? std::array<unsigned char,4>{43,68,53,255} : std::array<unsigned char,4>{28,41,36,255});text(120,237,1040,24,name_+(focus_==0 ? "|" : ""));if(click_ && hit(100,224,1080,60)) { focus_=0;click_=false; }
        if(definition_.maps.empty()) {
            text(100,300,1080,20,"Seed (texto ou numero; vazia = aleatoria)");rectangle(100,336,1080,60,focus_==1 ? std::array<unsigned char,4>{43,68,53,255} : std::array<unsigned char,4>{28,41,36,255});text(120,349,1040,24,seed_+(focus_==1 ? "|" : ""));if(click_ && hit(100,336,1080,60)) { focus_=1;click_=false; }
            std::string generator="Nenhum gerador instalado";if(!definition_.generators.empty()) { auto g=definition_.generators.begin();std::advance(g,generatorIndex_);generator=g->second.name; }
            button(24,100,428,1080,56,"Gerador: "+generator);
        } else text(100,315,1080,24,"Mapa: ShardBound");
        button(20,100,550,480,60,"Criar e jogar");button(21,600,550,220,60,"Voltar");
        text(100,492,1080,16,"Mesma seed + mesmo gerador = mesmo terreno. TAB alterna os campos.");
    } else if(screen_==Screen::Settings) {
        button(40,100,205,520,56,std::string("VSync: ")+(settings_.vsync ? "ligado" : "desligado"));button(41,660,205,520,56,std::string("Tela cheia: ")+(settings_.fullscreen ? "sim" : "nao"));
        button(42,100,281,520,56,std::string("Sombras: ")+(settings_.shadows ? "ligadas" : "desligadas"));button(43,660,281,520,56,"Escala de renderizacao: "+std::to_string(int(settings_.scale*100))+"%");
        button(44,100,357,520,56,"Distancia: "+std::to_string(settings_.viewDistance)+" chunks");text(660,370,520,22,"Volume: "+std::to_string(int(std::round(settings_.volume*100)))+"%");
        button(45,660,421,245,56,"Diminuir volume");button(46,935,421,245,56,"Aumentar volume");button(49,100,550,300,60,"Voltar");
        button(77,100,485,520,44,std::string("Diagnostico de desempenho: ")+(settings_.performance?"ligado":"desligado"));button(78,660,485,520,44,"Exportar relatorio de desempenho");button(80,420,550,360,60,"Controles e interface");button(81,800,550,380,60,"Canais de audio");
    } else if(screen_==Screen::Controls) {
        const auto ids=actions(input_);const auto bindings=input_.bindings();
        text(100,168,1080,18,rebinding_.empty()?"Clique numa acao. Delete restaura seu padrao; Escape cancela.":"Pressione a tecla ou botao para: "+rebinding_);
        for(size_t i=0;i<5 && controlsPage_*5+i<ids.size();++i) {const auto& id=ids[controlsPage_*5+i];button(500+int(i),100,205+int(i)*54,1080,46,actionLabel(id)+": "+bindingText(bindings.at(id)));}
        button(86,100,490,230,44,"Sensibilidade -");button(87,350,490,230,44,"+ "+std::to_string(int(std::round(settings_.sensitivity*100)))+"%");button(88,660,490,520,44,"Escala da interface: "+std::to_string(int(settings_.uiScale*100))+"%");
        button(83,100,550,220,60,"Voltar");button(82,340,550,440,60,"Restaurar controles");button(84,800,550,180,60,"Anterior");button(85,1000,550,180,60,"Proximo");
    } else if(screen_==Screen::Audio) {
        const float volumes[]={settings_.effectsVolume,settings_.musicVolume,settings_.interfaceVolume};const char* names[]={"Efeitos","Musica","Interface"};
        for(int i=0;i<3;++i) {text(100,210+i*90,500,26,std::string(names[i])+": "+std::to_string(int(std::round(volumes[i]*100)))+"%");button(450+i*2,660,205+i*90,245,56,"Diminuir");button(451+i*2,935,205+i*90,245,56,"Aumentar");}button(83,100,550,300,60,"Voltar");
    } else if(screen_==Screen::Mods) {
        if(profiles_) {
            button(90,100,170,600,44,"Perfil: "+profiles_->active()+" (alternar)");button(99,720,170,220,44,"Renomear");button(91,960,170,220,44,"Duplicar");
            const auto ids=profileIds();
            if(modsPage_*4>=ids.size()) modsPage_=0;
            for(size_t i=0;i<4 && modsPage_*4+i<ids.size();++i) {const auto& id=ids[modsPage_*4+i];const auto m=std::find_if(profiles_->catalog().begin(),profiles_->catalog().end(),[&](const auto& value){return value.id==id;});std::string label=(!profiles_->available(id)?"[Indisponivel] ":profiles_->enabled().contains(id)?"[Ativo] ":"[Desligado] ")+id;std::string dependencies="Pacote nao instalado";
                if(m!=profiles_->catalog().end()) {label+=" "+versionText(m->version)+" | "+m->name;dependencies="Dependencias: ";for(const auto& d:m->dependencies) dependencies+=d.id+" "+d.operation+" "+versionText(d.version)+(d.optional?" (opcional) ":" ");}
                button(600+int(i),100,230+int(i)*76,1080,44,label);text(110,274+int(i)*76,1060,16,dependencies);
            }
            const auto issues=profiles_->issues();if(!issues.empty() || !modDiagnostics_.empty()) button(95,100,610,1080,28,"Ver diagnosticos de mods e dependencias");
        }
        button(11,100,550,240,50,"Voltar");button(92,800,550,180,50,"Anterior");button(93,1000,550,180,50,"Proximo");
        if(!profileWorld_.empty()) button(94,360,550,420,50,"Aplicar ao mundo com backup");
    } else if(screen_==Screen::ProfileName) {
        text(100,210,1080,22,"Nome do perfil");rectangle(100,250,1080,60,{43,68,53,255});text(120,265,1040,24,profileName_+"|");button(79,100,550,440,50,"Salvar nome");button(98,560,550,440,50,"Cancelar");
    } else if(screen_==Screen::ModIssues) {
        std::vector<std::string> errors;for(const auto& d:modDiagnostics_) errors.push_back(d.mod+" | "+d.code+" | "+d.message);if(profiles_) {const auto issues=profiles_->issues();errors.insert(errors.end(),issues.begin(),issues.end());}
        for(size_t i=0;i<4 && issuesPage_*4+i<errors.size();++i) text(100,220+int(i)*74,1080,20,errors[issuesPage_*4+i],{255,190,160,255});
        button(98,100,550,240,50,"Voltar aos mods");button(96,800,550,180,50,"Anterior");button(97,1000,550,180,50,"Proximo");
    } else if(screen_==Screen::Pause) { button(30,100,230,480,60,saveError_.empty() ? "Retomar" : "Continuar sem salvar");button(31,100,310,480,60,"Salvar mundo");button(2,100,390,480,60,"Configuracoes");button(32,100,470,480,60,"Salvar e voltar ao inicio"); }
    if(!saveError_.empty()) {
        text(100,612,1080,20,"Nao foi possivel salvar. Tente novamente ou continue sem salvar.",{255,170,150,255});
        text(100,644,1080,18,saveError_.substr(0,200),{233,194,122,255});
    } else if(!message_.empty()) text(100,644,1080,18,message_.substr(0,200),{233,194,122,255});
}
TextureHandle FrontEnd::texture() {
    if(commands_!=previous_ || !texture_) { texture_=renderer_.createTexture(1280,720,canvasImage(commands_),false,false);previous_=commands_; }return texture_;
}
}
