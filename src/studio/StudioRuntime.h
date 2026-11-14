#pragma once
#include "ecs/World.h"
#include "audio/AudioSystem.h"
namespace game {
struct StudioStats { size_t particles=0,trails=0,flashes=0,glows=0,soundsStarted=0; };
class StudioRuntime {
public:
    StudioRuntime(World& world,Renderer& renderer,AudioSystem& audio);
    ~StudioRuntime();
    void play(EntityId entity,const std::string& clip,const std::string& mode="authored",float speed=1);
    void stop(EntityId entity);
    void seek(EntityId entity,double time);
    void pause(EntityId entity,bool paused);
    void setSuspended(bool suspended);
    void attach(EntityId equipment,EntityId player,bool drivePlayer=true);
    void detach(EntityId equipment);
    bool hasClip(EntityId entity,const std::string& clip) const;
    std::string currentClip(EntityId entity) const;
    double time(EntityId entity) const;
    void update(double delta);
    std::vector<RenderItem> render(const Camera& camera);
    StudioStats stats() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
