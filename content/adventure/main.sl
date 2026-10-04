import "definitions.sl"
import "equipment.sl"
import "regions.sl"
import "world.sl"
import "combat_visuals.sl"
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
    state.discoveries = []
    state.built_regions = []
    state.equipped = "adventure:IronSword"
    state.equipment_schema = 1
    state.items = []
    state.next_item_uid = 1
    state.equipped_uid = 0
    state.attack = 0
    state.winding = false
    state.swing = get_definition("Items", IronSword)
    state.aim = vec3(0, 0, -1)
    keyboard_movement_only(true)
    set_camera_offset(vec3(7, 10, -10))
    validate_world_content()
    build_world()
    ensure_equipment()
    refresh_hud()
    log("ShardBound playable world ready")

on_fixed_update(delta):
    explore_tick(delta)
    combat_tick(delta)

on_update(delta):
    ensure_equipment()
    handle_input()
    animate_hero(delta)
    animate_combat_visuals(delta)
    hud_time = hud_time - delta
    if hud_time <= 0:
        refresh_hud()
        hud_time = 0.1
