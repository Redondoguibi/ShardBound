import "definitions.sl"
import "world.sl"
import "exploration.sl"
import "combat.sl"
import "interface.sl"
import "persistence.sl"

@Events
on_start():
    state = record()
    state.schema = 1
    state.gold = 0
    state.combat = 0
    state.hub = false
    state.forest = false
    state.grove = false
    state.equipped = "adventure:IronSword"
    state.attack = 0
    state.winding = false
    state.swing = get_definition("Items", IronSword)
    state.aim = vec3(0, 0, -1)
    keyboard_movement_only(true)
    set_camera_offset(vec3(7, 10, -10))
    build_world()
    refresh_hud()
    log("ShardBound playable world ready")

on_fixed_update(delta):
    explore_tick(delta)
    combat_tick(delta)

on_update(delta):
    handle_input()
    animate_hero(delta)
    hud_time = hud_time - delta
    if hud_time <= 0:
        refresh_hud()
        hud_time = 0.1
