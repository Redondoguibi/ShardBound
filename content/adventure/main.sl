import "definitions.sl"
import "equipment.sl"
import "regions.sl"
import "world.sl"
import "scenery.sl"
import "combat_visuals.sl"
import "exploration.sl"
import "loot.sl"
import "combat.sl"
import "ui_canvas.sl"
import "ui_inventory.sl"
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
    state.encounters = []
    state.equipped = "adventure:IronSword"
    state.equipment_schema = 1
    state.items = []
    state.next_item_uid = 1
    state.equipped_uid = 0
    state.attack = 0
    state.winding = false
    state.swing = get_definition("Items", IronSword)
    state.aim = vec3(0, 0, -1)
    pause_game(false)
    audio_volume(volume)
    keyboard_movement_only(true)
    set_camera_offset(vec3(7, 10, -10))
    validate_world_content()
    build_world()
    configure_presentation()
    ensure_equipment()
    refresh_hud()
    log("ShardBound playable world ready")

on_fixed_update(delta):
    if paused:
        return
    explore_tick(delta)
    combat_tick(delta)

on_update(delta):
    ensure_equipment()
    handle_input()
    presentation_tick(delta)
    if not paused:
        animate_hero(delta)
        animate_combat_visuals(delta)
    hud_time = hud_time - delta
    if hud_time <= 0:
        refresh_hud()
        hud_time = 0.1

on_ready():
    title_open = true
    paused = true
    pause_game(true)
    refresh_hud()
