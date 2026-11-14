#include "game/PlayerController.h"
#include "platform/InputSystem.h"
#include "core/Diagnostic.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void wallJumpRegression(float step,game::Vec3 direction) {
    using namespace game;
    World world; PhysicsSystem physics(world); PlayerController player(world,physics);
    auto box=[&](Vec3 position,Vec3 half,bool dynamic) { auto id=world.create();world.add(id,TransformComponent{{position}});physics.attachBox(id,half,dynamic);return id; };
    box({0,-.5f,0},{10,.5f,10},false);
    const bool alongX=direction.x!=0;
    box({direction.x*1.5f,4,direction.z*1.5f},alongX?Vec3{.5f,4,8}:Vec3{8,4,.5f},false);
    const auto actor=box({0,1,0},{.4f,1,.3f},true);player.possess(actor,5);player.movementHeading(0.f);physics.setGravityFactor(actor,1.5f);
    for(int i=0;i<int(.5f/step);++i) physics.step(step);
    require(player.jump(6),"Wall jump fixture did not start grounded");
    bool falling=false,touchedWall=false;float peak=1;
    for(int i=0;i<int(2.5f/step);++i) {
        player.move(direction.x,direction.z);physics.step(step);
        const auto p=world.get<TransformComponent>(actor).local.position;
        peak=std::max(peak,p.y);falling|=p.y>1.15f && physics.velocity(actor).y<-.5f;
        touchedWall|=alongX?std::abs(p.x)>.5f:std::abs(p.z)>.6f;
        if(p.y>1.2f) require(!physics.grounded(actor),"Wall face counted as ground");
        require((alongX?std::abs(p.x):std::abs(p.z))<.76f,"Player crossed the wall");
    }
    require(falling && physics.grounded(actor),"Holding movement into a wall left the player suspended");
    require(touchedWall && peak>1.8f,"Wall contact cut the jump short");
    require(player.jump(6),"Player could not jump again after landing against a wall");
}
void blockFallAndSlideRegression() {
    using namespace game;
    World world;PhysicsSystem physics(world);PlayerController player(world,physics);
    auto box=[&](Vec3 position,Vec3 half,bool dynamic) { auto id=world.create();world.add(id,TransformComponent{{position}});physics.attachBox(id,half,dynamic);return id; };
    box({0,-.5f,0},{10,.5f,10},false);box({1.5f,.5f,0},{.5f,.5f,.5f},false);
    const auto actor=box({.5f,1.5f,0},{.32f,.9f,.32f},true);player.possess(actor,5);player.movementHeading(0.f);physics.setGravityFactor(actor,1.5f);
    bool fell=false;for(int i=0;i<30;++i) {player.move(1,0);physics.step(1.f/60);fell|=physics.velocity(actor).y<-.5f;}
    require(fell && physics.grounded(actor),"Falling player stuck to the side of a single block");
    box({1.5f,4,4},{.5f,4,4},false);physics.teleport(actor,{.5f,.9f,2});
    for(int i=0;i<30;++i) {player.move(0,0);physics.step(1.f/60);}
    require(player.jump(6),"Sliding fixture did not start grounded");
    for(int i=0;i<90;++i) {player.move(1,1);physics.step(1.f/60);}
    const auto slid=world.get<TransformComponent>(actor).local.position;
    require(slid.x<.72f && slid.z>6 && physics.grounded(actor),"Diagonal movement failed to slide and land along the wall");
    for(int i=0;i<15;++i) {player.move(-1,0);physics.step(1.f/60);}
    require(world.get<TransformComponent>(actor).local.position.x<0,"Player could not move away from the wall");
}
void movementFrictionScopeRegression() {
    using namespace game;
    PhysicsWorld physics;
    physics.box({0,-.5f,0},{10,.5f,10},false);physics.box({1.5f,4,0},{.5f,4,8},false);
    const auto driven=physics.box({.6f,3,-2},{.4f,1,.3f},true),ordinary=physics.box({.6f,3,2},{.4f,1,.3f},true);
    for(int i=0;i<20;++i) {
        physics.setMovementVelocity(driven,{5,physics.velocity(driven).y,0});
        physics.setVelocity(ordinary,{5,physics.velocity(ordinary).y,0});physics.step(1.f/60);
    }
    require(physics.position(driven).y<2.6f && physics.position(ordinary).y>2.9f,"Movement friction override affected unrelated bodies or did not allow falling");
    const auto height=physics.position(driven).y;
    for(int i=0;i<60;++i) {physics.setVelocity(driven,{5,physics.velocity(driven).y,0});physics.step(1.f/60);}
    require(physics.position(driven).y>height-.2f,"Movement friction override leaked into later ordinary velocity updates");
    const auto floorBody=physics.box({-3,1,0},{.4f,1,.3f},true);
    for(int i=0;i<30;++i) physics.step(1.f/60);
    physics.setMovementVelocity(floorBody,{3,0,0});physics.step(1.f/60);
    for(int i=0;i<60;++i) physics.step(1.f/60);
    require(std::abs(physics.velocity(floorBody).x)<1.2f,"Movement override removed floor friction");
}
int main() {
    using namespace game;
    try {
        for(const float step:{1.f/30,1.f/60,1.f/120})
            for(const auto direction:{Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,0,1},Vec3{0,0,-1}}) wallJumpRegression(step,direction);
        blockFallAndSlideRegression();movementFrictionScopeRegression();
        World world; PhysicsSystem physics(world); PlayerController player(world,physics);
        auto box=[&](Vec3 position,Vec3 half,bool dynamic) { auto id=world.create(); world.add(id,TransformComponent{{position}}); physics.attachBox(id,half,dynamic); return id; };
        box({0,-.5f,0},{10,.5f,10},false); auto actor=box({0,1,0},{.4f,1,.3f},true); player.possess(actor,4);
        for(int i=0;i<30;++i) physics.step(1.0f/60);
        require(physics.grounded(actor),"Standing actor is not grounded");
        player.move(1,1); auto v=physics.velocity(actor); require(std::abs(std::hypot(v.x,v.z)-4)<.001f,"Diagonal speed differs");
        player.updateCamera(1.0f/60); const auto cameraY=player.camera().target.y;
        require(player.jump(6),"Grounded jump rejected"); require(!player.jump(6),"Immediate double jump accepted");
        float peak=0; bool falling=false;
        for(int i=0;i<120;++i) {
            player.move(0,0); physics.step(1.0f/60); auto p=world.get<TransformComponent>(actor).local.position;
            player.updateCamera(1.0f/60);
            if(p.y>1.2f && !physics.grounded(actor)) require(std::abs(player.camera().target.y-cameraY)<.01f,"Camera follows jump height abruptly");
            peak=std::max(peak,p.y); falling|=physics.velocity(actor).y<-.1f && !physics.grounded(actor);
            if(i==30) require(!player.jump(6),"Air jump accepted");
        }
        require(peak>2.4f && falling && physics.grounded(actor),"Jump did not rise, fall and land");
        physics.teleport(actor,{2,1,0}); player.updateCamera(1.0f/60);
        require(player.camera().target.x>0 && player.camera().target.x<2,"Camera did not smooth lateral movement");
        physics.teleport(actor,{30,1,0}); player.updateCamera(1.0f/60);
        require(player.camera().target.x==30,"Camera did not snap after distant travel");
        physics.teleport(actor,{0,1,0});
        player.setEnabled(false); require(!player.jump(6),"Disabled jump accepted"); player.move(1,0); require(physics.velocity(actor).x==0,"Disabled actor moves"); player.setEnabled(true);
        world.add(actor,HealthComponent{0,100}); require(!player.jump(6),"Dead actor jumps"); world.get<HealthComponent>(actor).current=100;
        bool rejected=false;
        try { physics.setGravityFactor(actor,-1); } catch(const DiagnosticError&) { rejected=true; }
        require(rejected,"Negative gravity factor accepted");
        auto fallingBox=box({5,6,0},{.3f,.3f,.3f},true);
        physics.setGravityFactor(actor,3.26197757f);
        physics.teleport(actor,{0,6,0}); physics.step(1.0f/60);
        require(physics.velocity(actor).y<-.52f && physics.velocity(actor).y>-.55f,"Per-body gravity was not applied");
        require(physics.velocity(fallingBox).y<-.15f && physics.velocity(fallingBox).y>-.18f,"Player gravity changed other bodies");
        physics.teleport(actor,{0,6,0}); require(!physics.grounded(actor) && physics.velocity(actor).y==0,"Teleport retained support/velocity");
        auto wall=box({0,4,2},{1,4,.25f},false); auto target=box({0,6,4},{.3f,.3f,.3f},true);
        require(!physics.clearPath(actor,target),"Sight crosses wall"); world.destroy(wall); physics.step(1.0f/60); require(physics.clearPath(actor,target),"Clear sight rejected");
        player.setViewOffset({7,10,-10}); player.move(0,1); v=physics.velocity(actor);
        require(v.x<0 && v.z>0 && std::abs(std::hypot(v.x,v.z)-4)<.001f,"WASD is not aligned to camera");
        InputSystem input; input.keyboardMovementOnly(true); Event e; e.type=EventType::PadAxis; e.x=1; input.process(e); require(input.moveX()==0,"PC movement accepts pad");
        e.type=EventType::KeyDown; e.key=Key::W; input.process(e); require(input.moveY()==1,"W does not move");
        e.type=EventType::FocusLost; input.process(e); require(input.moveY()==0,"Movement stuck on focus loss");
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
