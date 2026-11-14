#include "game/DeveloperCommands.h"
#include "scripting/DataConversion.h"
#include "core/Log.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <limits>
namespace game {
namespace {
using namespace script;
double numeric(const std::string& text) {
    double value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{} || end!=text.data()+text.size() || !std::isfinite(value)) throw std::runtime_error("Número inválido: "+text);return value;
}
std::string decimal(double n) { std::ostringstream s;s<<std::setprecision(15)<<n;return s.str(); }
std::vector<std::string> tokenize(std::string_view text,bool incomplete=false) {
    std::vector<std::string> result;std::string word;bool quoted=false,escaped=false,started=false;
    for(char c:text) {
        if(escaped) { if(c!='"' && c!='\\') throw std::runtime_error("Escape inválido no argumento");word+=c;escaped=false; }
        else if(c=='\\' && quoted) escaped=true;
        else if(c=='"') { quoted=!quoted;started=true; }
        else if(c==' ' && !quoted) { if(started) { result.push_back(std::move(word));word.clear();started=false; } }
        else { if(static_cast<unsigned char>(c)<32) throw std::runtime_error("Comandos devem ocupar uma única linha");word+=c;started=true; }
        if(result.size()>33 || word.size()>1024) throw std::runtime_error("Limite de argumentos excedido");
    }
    if((quoted || escaped) && !incomplete) throw std::runtime_error("Aspas não fechadas");
    if(started) result.push_back(std::move(word));else if(incomplete && !text.empty() && text.back()==' ') result.emplace_back();
    return result;
}
bool callable(const Value& value) { return std::holds_alternative<std::shared_ptr<Callable>>(value.data); }
std::string join(const std::vector<std::string>& values,const std::string& separator="\n") { std::string out;for(const auto& value:values) { if(out.size()>60000) { out+="\nLimite de saída atingido.";break; }if(!out.empty()) out+=separator;out+=value; }return out; }
bool identifier(std::string_view text,bool colon) {
    return !text.empty() && text.size()<=64 && std::all_of(text.begin(),text.end(),[&](unsigned char c){return (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-' || (colon && c==':');});
}
}
void DeveloperCommands::add(std::string name,Command command) {
    if(commands_.size()>=256 || !commands_.emplace(std::move(name),std::move(command)).second) throw std::runtime_error("Comando duplicado ou limite de 256 comandos atingido");
}
ChunkWorld& DeveloperCommands::requireTerrain() const { if(!terrain_) throw std::runtime_error("Este comando precisa de um mundo procedural aberto");return *terrain_; }
EntityId DeveloperCommands::actor() const { const auto id=player_.entity();if(!id) throw std::runtime_error("Jogador indisponível");return id; }
double DeveloperCommands::coordinate(const std::string& token,int axis,bool block) const {
    const auto position=world_.get<TransformComponent>(actor()).local.position;
    const double origin=axis==0 && terrain_?double(terrain_->originX()):axis==2 && terrain_?double(terrain_->originZ()):0;
    const double raw=origin+(axis==0?position.x:axis==1?position.y:position.z);const double base=block?std::floor(raw):raw;
    const bool relative=token.starts_with('~');const auto value=relative?(token.size()==1?0:numeric(token.substr(1))):numeric(token);
    const double resolved=relative?base+value:value;if(!std::isfinite(resolved) || std::abs(resolved)>1e12) throw std::runtime_error("Coordenada fora do limite seguro do mundo");return resolved;
}
Vec3 DeveloperCommands::localPosition(const std::vector<std::string>& a,size_t offset) const {
    const double x=coordinate(a.at(offset),0)-(terrain_?double(terrain_->originX()):0),y=coordinate(a.at(offset+1),1),z=coordinate(a.at(offset+2),2)-(terrain_?double(terrain_->originZ()):0);
    if(std::abs(x)>100000 || std::abs(y)>100000 || std::abs(z)>100000) throw std::runtime_error("Destino distante demais: limite de 100000 unidades por operação");return {float(x),float(y),float(z)};
}
std::vector<std::string> DeveloperCommands::identifiers(const std::string& type) const {
    std::vector<std::string> result;
    if(type=="block") { if(terrain_) for(const auto& [id,block]:terrain_->generator().definition().blocks) { (void)block;result.push_back(id); }result.push_back("air"); }
    else {
        const auto category=type=="item"?"Items":type=="entity"?"Entities":"Attributes";
        result=runtime_.registry().ids(category);
    }
    std::sort(result.begin(),result.end());return result;
}
DeveloperCommands::DeveloperCommands(Runtime& runtime,World& world,PhysicsSystem& physics,PlayerController& player):runtime_(runtime),world_(world),physics_(physics),player_(player) {
    const auto builtin=[this](std::string name,std::string usage,std::string description,std::vector<Argument> args,auto action) { add(std::move(name),{std::move(usage),std::move(description),"core",std::move(args),action}); };
    builtin("help","/help [comando]","Mostra ajuda sobre os comandos",{{"comando","command",true,{}}},[this](const auto& a) {
        if(a.empty()) { std::vector<std::string> lines;for(const auto& [id,c]:commands_) { (void)id;lines.push_back(c.usage+" — "+c.description); }return join(lines); }
        const auto name=a[0].starts_with('/')?a[0].substr(1):a[0];const auto it=commands_.find(name);if(it==commands_.end()) throw std::runtime_error("Comando desconhecido: "+name);return it->second.usage+" — "+it->second.description;
    });
    builtin("list","/list [items|entities|blocks]","Lista comandos ou conteúdo registrado",{{"tipo","enum",true,{"items","entities","blocks"}}},[this](const auto& a) {
        if(a.empty()) { std::vector<std::string> lines;for(const auto& [id,c]:commands_) { (void)c;lines.push_back("/"+id); }return join(lines); }
        const auto values=identifiers(a[0]=="items"?"item":a[0]=="entities"?"entity":"block");return values.empty()?std::string("Nenhum conteúdo registrado."):join(values);
    });
    builtin("camera","/camera <default|thirdperson|free>","Troca a câmera de desenvolvimento",{{"modo","enum",false,{"default","thirdperson","free"}}},[this](const auto& a) { if(!changeCamera) throw std::runtime_error("Câmera indisponível");changeCamera(a[0]);return "Câmera: "+a[0]; });
    builtin("pos","/pos","Mostra a posição global do jogador",{},[this](const auto&) { const auto p=world_.get<TransformComponent>(actor()).local.position;return decimal(double(p.x)+(terrain_?double(terrain_->originX()):0))+" "+decimal(p.y)+" "+decimal(double(p.z)+(terrain_?double(terrain_->originZ()):0)); });
    builtin("tp","/tp <x> <y> <z>","Teletransporta o jogador; ~ aceita coordenadas relativas",{{"x","string"},{"y","string"},{"z","string"}},[this](const auto& a) { const auto p=localPosition(a,0);physics_.teleport(actor(),p);return std::string("Jogador teletransportado."); });
    builtin("give","/give <mod:item> [quantidade]","Entrega itens registrados ao jogador",{{"item","item"},{"quantidade","integer",true,{}}},[this](const auto& a) { const double count=a.size()==2?numeric(a[1]):1;if(count<1 || count>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("Quantidade fora do intervalo permitido");runtime_.call("give_item",{entityValue(world_,actor()),Value(a[0]),Value(count)});return decimal(count)+" × "+a[0]; });
    builtin("spawn","/spawn <mod:entity> [x y z]","Cria uma entidade registrada",{{"entidade","entity"},{"x","string",true,{}},{"y","string",true,{}},{"z","string",true,{}}},[this](const auto& a) {
        if(a.size()!=1 && a.size()!=4) throw std::runtime_error("Informe as três coordenadas ou nenhuma");
        Vec3 p;if(a.size()==4) p=localPosition(a,1);else { p=world_.get<TransformComponent>(actor()).local.position;p.x+=2; }
        size_t persistent=0;for(auto id:world_.entities()) persistent+=!terrain_ || !terrain_->transient(id);
        if(persistent>=4096) throw std::runtime_error("Limite de entidades persistentes atingido");
        auto entity=runtime_.call("spawn",{Value(a[0]),runtime_.call("vec3",{Value(double(p.x)),Value(double(p.y)),Value(double(p.z))})});
        const auto errors=runtime_.emit("on_developer_spawn",{entity});
        if(!errors.empty()) {for(const auto& error:errors) LOG_ERROR("SCRIPT",DiagnosticError(error).what());return std::string("Erro: entidade criada, mas um mod falhou na inicialização: ")+DiagnosticError(errors.front()).what();}
        return "Entidade criada: "+a[0];
    });
    builtin("set","/set <x1> <y1> <z1> <x2> <y2> <z2> <mod:block>","Edita até 4096 blocos em uma transação; air remove",{{"x1","string"},{"y1","string"},{"z1","string"},{"x2","string"},{"y2","string"},{"z2","string"},{"bloco","block"}},[this](const auto& a) {
        auto& terrain=requireTerrain();std::int64_t low[3],high[3];
        for(int axis=0;axis<3;++axis) { const auto x=coordinate(a[size_t(axis)],axis,true),y=coordinate(a[size_t(axis+3)],axis,true);if(x!=std::floor(x) || y!=std::floor(y)) throw std::runtime_error("A edição exige coordenadas inteiras");low[axis]=std::int64_t(std::min(x,y));high[axis]=std::int64_t(std::max(x,y)); }
        if(low[1]<0 || high[1]>=TerrainGenerator::maxHeight) throw std::runtime_error("Altura de edição deve estar entre 0 e 47");
        std::uint64_t volume=1;for(int axis=0;axis<3;++axis) { const auto extent=std::uint64_t(high[axis]-low[axis])+1;if(extent>4096 || volume>4096/extent) throw std::runtime_error("Limite de 4096 blocos por edição");volume*=extent; }
        BlockEdits edits;for(auto x=low[0];x<=high[0];++x) for(auto y=low[1];y<=high[1];++y) for(auto z=low[2];z<=high[2];++z) edits[{x,int(y),z}]=a[6]=="air"?"":a[6];
        return std::to_string(terrain.editBlocks(edits))+" blocos alterados.";
    });
    builtin("undo","/undo","Desfaz a última edição de terreno",{},[this](const auto&) { return std::string(requireTerrain().undoEdits()?"Edição desfeita.":"Nada para desfazer."); });
    builtin("redo","/redo","Refaz uma edição de terreno",{},[this](const auto&) { return std::string(requireTerrain().redoEdits()?"Edição refeita.":"Nada para refazer."); });
    builtin("seed","/seed","Mostra a seed original do mundo",{},[this](const auto&) { (void)requireTerrain();return "Seed: "+seed_; });
    builtin("attribute","/attribute <mod:atributo> <get|set> [valor]","Consulta valor base e efetivo ou altera a base",{{"atributo","attribute"},{"operação","enum",false,{"get","set"}},{"valor","number",true,{}}},[this](const auto& a) {
        if((a[1]=="get" && a.size()!=2) || (a[1]=="set" && a.size()!=3)) throw std::runtime_error("Uso: /attribute mod:atributo get | /attribute mod:atributo set valor");
        const auto actorValue=entityValue(world_,actor());if(a[1]=="set") runtime_.call("set_attribute",{actorValue,Value(a[0]),Value(numeric(a[2]))});
        return a[0]+": base="+decimal(runtime_.call("get_base_attribute",{actorValue,Value(a[0])}).number())+" efetivo="+decimal(runtime_.call("get_attribute",{actorValue,Value(a[0])}).number());
    });
    runtime.bind("register_command",native([this](const auto& a,const auto& at) {
        if(a.size()!=3) fail(at,"DEV101","register_command(id, {description,arguments}, handler)");const auto id=a[0].string(at),owner=runtime_.currentMod();
        if(owner.empty() || !identifier(id,true) || AssetId(id).nameSpace()!=owner) fail(at,"DEV101","Command ID must belong to the calling mod");
        if(!callable(a[2])) fail(at,"DEV101","Command handler must be callable");(void)toData(a[1]);Command command;command.owner=owner;command.description=a[1].member("description",at).string(at);
        if(command.description.size()>256) fail(at,"DEV101","Command description exceeds 256 bytes");
        auto args=a[1].member("arguments",at);const auto rows=std::get_if<std::shared_ptr<List>>(&args.data);if(!rows || (*rows)->values.size()>32) fail(at,"DEV101","Expected at most 32 argument descriptors");
        bool optional=false;std::set<std::string> names;command.usage="/"+id;
        for(const auto& value:(*rows)->values) {
            Argument arg;arg.name=value.member("name",at).string(at);arg.type=value.member("type",at).string(at);arg.optional=value.member("optional",at).boolean(at);
            if(!identifier(arg.name,false) || !names.insert(arg.name).second || (optional && !arg.optional)) fail(at,"DEV101","Invalid argument name/order");optional=arg.optional;
            const std::set<std::string> types{"string","number","integer","item","entity","block","attribute","enum"};if(!types.contains(arg.type)) fail(at,"DEV101","Unknown command argument type");
            if(arg.type=="enum") {
                auto choices=value.member("choices",at);const auto list=std::get_if<std::shared_ptr<List>>(&choices.data);if(!list || (*list)->values.empty() || (*list)->values.size()>64) fail(at,"DEV101","Enum needs 1 to 64 choices");
                for(const auto& item:(*list)->values) { const auto choice=item.string(at);if(!identifier(choice,true)) fail(at,"DEV101","Invalid enum choice");arg.choices.push_back(choice); }
            }
            command.usage+=(arg.optional?" [":" <")+arg.name+(arg.optional?"]":">");command.arguments.push_back(std::move(arg));
        }
        const auto handler=a[2];const auto descriptors=command.arguments;
        command.action=[this,handler,descriptors](const std::vector<std::string>& tokens) {
            auto args=object();for(size_t i=0;i<descriptors.size();++i) {
                Value value;if(i<tokens.size()) value=(descriptors[i].type=="number" || descriptors[i].type=="integer")?Value(numeric(tokens[i])):Value(tokens[i]);args.setMember(descriptors[i].name,value,{});
            }
            const auto result=runtime_.invoke(handler,{args,entityValue(world_,actor())});if(std::holds_alternative<std::monostate>(result.data)) return std::string("Comando executado.");const auto text=result.string();if(text.size()>16384) throw std::runtime_error("Saída do comando excede 16384 bytes");return text;
        };
        add(id,std::move(command));return Value{};
    }));
    runtime.bind("unregister_command",native([this](const auto& a,const auto& at) {
        if(a.size()!=1) fail(at,"DEV101","unregister_command(id)");const auto found=commands_.find(a[0].string(at));if(found==commands_.end()) return Value(false);
        if(runtime_.currentMod().empty() || found->second.owner!=runtime_.currentMod()) fail(at,"DEV101","Command belongs to another owner");commands_.erase(found);return Value(true);
    }));
}
std::string DeveloperCommands::execute(const std::string& line) {
    try {
        if(line.size()>1024 || !line.starts_with('/')) throw std::runtime_error("Comando deve começar com / e ter até 1024 bytes");(void)toData(Value(line));
        auto tokens=tokenize(std::string_view(line).substr(1));if(tokens.empty()) throw std::runtime_error("Digite um comando; /help mostra a ajuda");
        const auto found=commands_.find(tokens.front());if(found==commands_.end()) throw std::runtime_error("Comando desconhecido: "+tokens.front());const auto command=found->second;tokens.erase(tokens.begin());
        const auto required=std::count_if(command.arguments.begin(),command.arguments.end(),[](const auto& a){return !a.optional;});
        if(tokens.size()<size_t(required) || tokens.size()>command.arguments.size()) throw std::runtime_error("Uso: "+command.usage);
        for(size_t i=0;i<tokens.size();++i) {
            const auto& arg=command.arguments[i];if(arg.type=="number" || arg.type=="integer") { const auto n=numeric(tokens[i]);if(arg.type=="integer" && n!=std::floor(n)) throw std::runtime_error("Esperado inteiro: "+arg.name); }
            else if(arg.type=="enum" && std::find(arg.choices.begin(),arg.choices.end(),tokens[i])==arg.choices.end()) throw std::runtime_error("Uso: "+command.usage);
            else if(arg.type=="item" || arg.type=="entity" || arg.type=="block" || arg.type=="attribute") { const auto ids=identifiers(arg.type);if(std::find(ids.begin(),ids.end(),tokens[i])==ids.end()) throw std::runtime_error("Identificador desconhecido: "+tokens[i]); }
        }
        return command.action(tokens);
    } catch(const std::exception& error) { return std::string("Erro: ")+error.what(); }
}
std::vector<std::string> DeveloperCommands::complete(const std::string& prefix) const {
    try {
        if(prefix.size()>1024 || !prefix.starts_with('/')) return {};auto tokens=tokenize(std::string_view(prefix).substr(1),true);if(tokens.empty()) tokens.emplace_back();
        std::vector<std::string> choices;const auto partial=tokens.back();
        if(tokens.size()==1) for(const auto& [id,c]:commands_) { (void)c;choices.push_back(id); }
        else {
            const auto found=commands_.find(tokens[0]);if(found==commands_.end() || tokens.size()-2>=found->second.arguments.size()) return {};
            const auto& arg=found->second.arguments[tokens.size()-2];if(arg.type=="enum") choices=arg.choices;
            else if(arg.type=="command") for(const auto& [id,c]:commands_) { (void)c;choices.push_back(id); }
            else if(arg.type=="item" || arg.type=="entity" || arg.type=="block" || arg.type=="attribute") choices=identifiers(arg.type);
        }
        std::vector<std::string> result;const auto last=prefix.find_last_of(' ');const auto head=tokens.size()==1?std::string("/"):prefix.substr(0,last+1);
        for(const auto& choice:choices) if(choice.starts_with(partial)) { result.push_back(head+choice);if(result.size()==64) break; }return result;
    }catch(const std::exception&) { return {}; }
}
}
