#include "ecs/World.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        game::World world;
        const auto parent=world.create("parent"),child=world.create("child");
        world.add(parent,game::TransformComponent{{{3,0,0}}});
        world.add(child,game::TransformComponent{{{2,0,0}}}); world.setParent(child,parent);
        require(std::abs(world.worldMatrix(child).values[12]-5)<0.001f,"Parent transform not propagated");
        bool rejected=false; try { world.setParent(parent,child); } catch (...) { rejected=true; }
        require(rejected,"Parent cycle accepted");
        world.remove<game::TransformComponent>(child); require(!world.has<game::TransformComponent>(child),"Component removal failed");
        world.destroy(parent); require(!world.alive(parent) && !world.parent(child),"Destroy did not detach children");
        require(world.create().value!=parent.value,"Destroyed ID reused");
        rejected=false; try { world.get<game::NameComponent>(parent); } catch (...) { rejected=true; }
        require(rejected,"Stale entity accepted");
        require(world.find("child")==child,"Name lookup failed");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
