#include "scripting/EngineServices.h"
#include <cmath>
#include <algorithm>
#include <numbers>
namespace game {
namespace {
const Data::Object& object(const Data& value) {const auto* p=std::get_if<Data::Object>(&value.value);if(!p) throw std::runtime_error("Expected engine object");return *p;}
double number(const Data& value,double low,double high) {const auto* p=std::get_if<double>(&value.value);if(!p || !std::isfinite(*p) || *p<low || *p>high) throw std::runtime_error("Engine number outside permitted range");return *p;}
bool boolean(const Data& value) {const auto* p=std::get_if<bool>(&value.value);if(!p) throw std::runtime_error("Expected engine boolean");return *p;}
void keys(const Data& value,const std::vector<std::string>& allowed) {for(const auto& [key,field]:object(value)) {(void)field;if(std::find(allowed.begin(),allowed.end(),key)==allowed.end()) throw std::runtime_error("Unknown engine field: "+key);}}
Data color(Color value) {return Data{Data::Object{{"x",Data{double(value.r)}},{"y",Data{double(value.g)}},{"z",Data{double(value.b)}}}};}
Color color(const Data& value,float limit) {keys(value,{"x","y","z"});const auto& p=object(value);return {float(number(p.at("x"),0,limit)),float(number(p.at("y"),0,limit)),float(number(p.at("z"),0,limit)),1};}
Data environment(const Environment& env) {return Data{Data::Object{{"enabled",Data{env.enabled}},{"shadows",Data{env.shadows}},{"sky",color(env.sky)},{"sun",color(env.sun)},{"ambient",color(env.ambient)},{"fog_start",Data{double(env.fogStart)}},{"fog_end",Data{double(env.fogEnd)}}}};}
}
double engineNumber(const Data& context,const std::string& key) {return number(object(context).at(key),-1e9,1e9);}
bool engineBool(const Data& context,const std::string& key) {return boolean(object(context).at(key));}
Data engineVector(Vec3 value) {return Data{Data::Object{{"x",Data{double(value.x)}},{"y",Data{double(value.y)}},{"z",Data{double(value.z)}}}};}
Vec3 engineVector(const Data& value) {keys(value,{"x","y","z"});const auto& p=object(value);return {float(number(p.at("x"),-100000,100000)),float(number(p.at("y"),-100000,100000)),float(number(p.at("z"),-100000,100000))};}
Data engineCamera(const Camera& value) {return Data{Data::Object{{"eye",engineVector(value.eye)},{"target",engineVector(value.target)},{"near",Data{double(value.nearPlane)}},{"far",Data{double(value.farPlane)}},{"fov",Data{double(value.fov)}}}};}
Camera engineCamera(const Data& value) {
    keys(value,{"eye","target","near","far","fov"});const auto& p=object(value);Camera out;out.eye=engineVector(p.at("eye"));out.target=engineVector(p.at("target"));out.nearPlane=float(number(p.at("near"),.001,10000));out.farPlane=float(number(p.at("far"),.002,100000));out.fov=float(number(p.at("fov"),.1,3));
    if(out.nearPlane>=out.farPlane || std::hypot(out.eye.x-out.target.x,out.eye.z-out.target.z)<.0001f) throw std::runtime_error("Invalid engine camera projection or viewing direction");return out;
}
void validateEngineContext(const Data& context) {
    const auto& p=object(context);const auto stage=std::get<std::string>(p.at("stage").value);if(number(p.at("api"),1,1)!=1) throw std::runtime_error("Unsupported engine context");
    if(stage=="fixed" || stage=="update" || stage=="physics") {keys(context,{"api","stage","delta"});number(p.at("delta"),stage=="physics"?0.000001:0,.25);}
    else if(stage=="movement") {keys(context,{"api","stage","right","forward","player_key"});number(p.at("right"),-1,1);number(p.at("forward"),-1,1);if(!std::holds_alternative<std::string>(p.at("player_key").value)) throw std::runtime_error("Invalid player key");}
    else if(stage=="streaming") {keys(context,{"api","stage","distance","center"});const auto distance=number(p.at("distance"),2,5);if(distance!=std::floor(distance)) throw std::runtime_error("Streaming distance must be an integer");if(!std::holds_alternative<std::monostate>(p.at("center").value)) (void)engineVector(p.at("center"));}
    else if(stage=="render") {keys(context,{"api","stage","camera","world","terrain","effects","ui"});(void)engineCamera(p.at("camera"));for(const auto* name:{"world","terrain","effects","ui"}) boolean(p.at(name));}
    else throw std::runtime_error("Unknown engine context stage");
}
void bindEngineServices(EngineBridge& bridge,PhysicsSystem& physics,Renderer& renderer) {
    bridge.service=[&bridge,&physics,&renderer](const std::string& service,const Data& options)->Data {
        if(service=="configuration") {
            const auto rules=physics.rules();Data::Array lights;for(const auto& light:renderer.pointLights()) lights.push_back(Data{Data::Object{{"position",engineVector(light.position)},{"color",color(light.color)},{"range",Data{double(light.range)}},{"intensity",Data{double(light.intensity)}}}});
            Data::Array order;for(const auto& stage:bridge.fixedOrder) order.push_back(Data{stage});
            return Data{Data::Object{{"physics",Data{Data::Object{{"gravity",Data{double(rules.gravity)}},{"max_slope",Data{std::acos(double(rules.supportNormal))*180/std::numbers::pi}}}}},{"render",Data{Data::Object{{"environment",environment(renderer.environment())},{"lights",Data{std::move(lights)}}}}},{"scheduler",Data{Data::Object{{"fixed_order",Data{std::move(order)}}}}}}};
        }
        if(service=="physics") {
            keys(options,{"gravity","max_slope"});auto rules=physics.rules();const auto& fields=object(options);
            if(fields.contains("gravity")) rules.gravity=float(number(fields.at("gravity"),0,100));
            if(fields.contains("max_slope")) rules.supportNormal=float(std::cos(number(fields.at("max_slope"),1,80)*std::numbers::pi/180));physics.rules(rules);
        } else if(service=="render") {
            keys(options,{"environment","lights"});auto env=renderer.environment();auto lights=renderer.pointLights();const auto& fields=object(options);
            if(fields.contains("environment")) {
                const auto& specification=fields.at("environment");keys(specification,{"enabled","shadows","sky","sun","ambient","fog_start","fog_end"});
                for(const auto& [key,value]:object(specification)) {
                    if(key=="enabled") env.enabled=boolean(value);else if(key=="shadows") env.shadows=boolean(value);else if(key=="sky") env.sky=color(value,2);else if(key=="sun") env.sun=color(value,2);else if(key=="ambient") env.ambient=color(value,2);else if(key=="fog_start") env.fogStart=float(number(value,0,10000));else if(key=="fog_end") env.fogEnd=float(number(value,0,10000));
                }
                if(env.fogEnd<=env.fogStart) throw std::runtime_error("Fog end must exceed fog start");
            }
            if(fields.contains("lights")) {
                const auto* values=std::get_if<Data::Array>(&fields.at("lights").value);if(!values || values->size()>32) throw std::runtime_error("Engine point light limit is 32");lights.clear();
                for(const auto& value:*values) {keys(value,{"position","color","range","intensity"});const auto& light=object(value);lights.push_back({engineVector(light.at("position")),float(number(light.at("range"),.01,10000)),color(light.at("color"),32),float(number(light.at("intensity"),0,64))});}
            }
            renderer.scene(env,std::move(lights));
        } else if(service=="scheduler") {
            keys(options,{"fixed_order"});const auto& values=std::get<Data::Array>(object(options).at("fixed_order").value);std::vector<std::string> order;for(const auto& value:values) order.push_back(std::get<std::string>(value.value));auto sorted=order;std::sort(sorted.begin(),sorted.end());
            if(sorted!=std::vector<std::string>{"fixed","movement","physics"}) throw std::runtime_error("Fixed order must contain fixed, movement and physics exactly once");bridge.fixedOrder=std::move(order);
        } else throw std::runtime_error("Unknown configurable engine service");return {};
    };
}
}
