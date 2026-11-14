#include "scripting/StudioBridge.h"
#include "assets/StudioPackage.h"
namespace game {
void bindStudioApi(script::Runtime& runtime,World& world,StudioRuntime& studio) {
    using namespace script;
    auto arity=[](const auto& a,size_t n,const auto& at){if(a.size()!=n)fail(at,"STUDIO301","Expected "+std::to_string(n)+" arguments");};
    runtime.bind("animation_play",native([&,arity](const auto& a,const auto& at){if(a.size()<2||a.size()>4)fail(at,"STUDIO301","animation_play(entity, clip, mode?, speed?)");studio.play(requireEntity(a[0],world,at),a[1].string(at),a.size()>2?a[2].string(at):"authored",a.size()>3?float(a[3].number(at)):1);return Value{};}));
    runtime.bind("animation_stop",native([&,arity](const auto& a,const auto& at){arity(a,1,at);studio.stop(requireEntity(a[0],world,at));return Value{};}));
    runtime.bind("animation_seek",native([&,arity](const auto& a,const auto& at){arity(a,2,at);studio.seek(requireEntity(a[0],world,at),a[1].number(at));return Value{};}));
    runtime.bind("animation_pause",native([&,arity](const auto& a,const auto& at){arity(a,2,at);studio.pause(requireEntity(a[0],world,at),a[1].boolean(at));return Value{};}));
    runtime.bind("animation_has",native([&,arity](const auto& a,const auto& at){arity(a,2,at);return Value(studio.hasClip(requireEntity(a[0],world,at),a[1].string(at)));}));
    runtime.bind("animation_current",native([&,arity](const auto& a,const auto& at){arity(a,1,at);return Value(studio.currentClip(requireEntity(a[0],world,at)));}));
    runtime.bind("animation_time",native([&,arity](const auto& a,const auto& at){arity(a,1,at);return Value(studio.time(requireEntity(a[0],world,at)));}));
    runtime.bind("studio_attach",native([&,arity](const auto& a,const auto& at){arity(a,3,at);studio.attach(requireEntity(a[0],world,at),requireEntity(a[1],world,at),a[2].boolean(at));return Value{};}));
    runtime.bind("studio_detach",native([&,arity](const auto& a,const auto& at){arity(a,1,at);studio.detach(requireEntity(a[0],world,at));return Value{};}));
    runtime.bind("studio_projectile_visual",native([&,arity](const auto& a,const auto& at){arity(a,1,at);auto e=requireEntity(a[0],world,at);const auto& m=world.get<ModelComponent>(e).model;if(!m||!m->studio)return Value{};const auto& visual=m->studio->manifest["projectileVisual"];return visual.is_string()?Value(visual.get<std::string>()):Value{};}));
    runtime.bind("animation_clips",native([&,arity](const auto& a,const auto& at){arity(a,1,at);auto e=requireEntity(a[0],world,at);std::vector<Value> clips;if(world.has<ModelComponent>(e)){const auto& m=world.get<ModelComponent>(e).model;if(m){if(m->studio){for(const auto& c:m->studio->events["clips"])clips.emplace_back(c["name"].get<std::string>());}else for(const auto& c:m->hierarchy.clips)clips.emplace_back(c.name);}}return list(std::move(clips));}));
}
}
