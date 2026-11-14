@Entities
entity Actor {
    Assets:
        .model = "core:player"
}
entity Sword {
    Assets:
        .model = "studio_demo:crystal_sword"
}
entity Ground {
    Assets:
        .model = "core:grass"
}

@Globals
actor = null
sword = null

@Events
on_start():
    floor = spawn(Ground, vec3(0, -0.5, 0))
    set_scale(floor, vec3(6, 0.5, 6))
    actor = spawn(Actor, vec3(0, 0, 0))
    sword = spawn(Sword, vec3(0, 0, 0))
    studio_attach(sword, actor, true)
    animation_play(sword, "attack", "loop")
    log("Studio showcase ready: animated player, equipment, particles, trail, flash, glow and audio")
