import "definitions.sl"
import "player_attributes.sl"
import "interface.sl"
import "combat.sl"
import "equipment_stats.sl"
import "encounters.sl"

func equip_visual(id):
    spec = get_definition("Items", id)
    if is_studio_model(blade):
        animation_stop(blade)
        studio_detach(blade)
    set_model(blade, spec.model)
    if is_studio_model(blade):
        studio_attach(blade, avatar, true)
    else:
        set_parent(blade, avatar)
    set_scale(blade, spec.scale)
    attack_profile = null
    attack_time = 0
    winding = false
    combo_index = 0
    combo_time = 0
    state.weapon = id

func equip_armor_visual(spec):
    name = "equipped_" + spec.armor_slot
    piece = get_entity(name)
    if piece == null:
        piece = spawn(Blade, vec3(0, 0, 0))
        piece.name = name
        set_parent(piece, avatar)
    if is_studio_model(piece):
        studio_detach(piece)
    set_model(piece, spec.model)
    if is_studio_model(piece):
        studio_attach(piece, avatar, false)
    set_scale(piece, spec.scale)

func armor_bonus(add):
    bonus = record()
    bonus.add = add
    bonus.multiply = 1
    return bonus

func equip(id):
    spec = get_definition("Items", id)
    if record_has(spec, "armor_slot"):
        for candidate in item_instances(hero):
            if candidate.item == id:
                equip_instance(hero, spec.armor_slot, candidate.id)
                return
        options = record()
        options.durability = 1
        options.maximum = 1
        options.stats = record()
        options.stats.armor = armor_bonus(spec.protection)
        if record_has(spec, "movement_bonus"):
            record_set(options.stats, "foundation:movement_speed", armor_bonus(spec.movement_bonus))
        if record_has(spec, "health_bonus"):
            record_set(options.stats, "foundation:max_health", armor_bonus(spec.health_bonus))
        options.affixes = record()
        created = create_item_instance(hero, id, options)
        equip_instance(hero, spec.armor_slot, created)
        return
    current = equipped_instance(hero, "weapon")
    if current != null:
        if item_instance(hero, current).item == id:
            equip_visual(id)
            return
    if equipped_item(hero, "weapon") == id:
        equip_visual(id)
        return
    for candidate in item_instances(hero):
        if candidate.item == id and candidate.durability > 0:
            if current == null:
                equip_slot(hero, "weapon", null)
            equip_instance(hero, "weapon", candidate.id)
            equip_visual(id)
            return
    if available_item_count(hero, id) <= 0:
        notice = "Este item esta reservado ou precisa de reparo."
        return
    if current != null:
        equip_instance(hero, "weapon", null)
    equip_slot(hero, "weapon", id)
    equip_visual(id)

@Events
on_instance_equipped(actor, slot, instance):
    if actor == hero:
        apply_player_attributes()
    if actor == hero and slot == "weapon" and instance != null:
        equip_visual(item_instance(hero, instance).item)
    if actor == hero and slot != "weapon":
        if instance == null:
            piece = get_entity("equipped_" + slot)
            if piece != null:
                destroy(piece)
        else:
            spec = get_definition("Items", item_instance(hero, instance).item)
            if record_has(spec, "armor_slot"):
                equip_armor_visual(spec)

@Events
on_world_start():
    state = record()
    state.schema = 1
    state.gold = 0
    state.defeated = []
    state.stored_drops = []
    state.weapon = get_definition("Rules", Gameplay).starting_weapon
    stats = get_definition("Entities", Actor)
    start = world_spawn()
    hero = spawn(Actor, vec3(start.x, start.y + stats.body.y + 0.05, start.z))
    hero.name = "player"
    add_box(hero, stats.body, true)
    set_health(hero, stats.max_health)
    set_gravity_factor(hero, stats.gravity)
    set_player(hero, stats.speed)
    set_player_step_height(1.05)
    avatar = spawn(Avatar, vec3(0, -stats.body.y, 0))
    avatar.name = "player_visual"
    set_parent(avatar, hero)
    blade = spawn(Blade, vec3(0.375, 0.4, 0))
    blade.name = "player_weapon"
    set_parent(blade, avatar)
    give_item(hero, state.weapon, 1)
    equip(state.weapon)
    for entry in get_definitions("Items"):
        if record_has(entry.data, "editor_grant") and entry.data.editor_grant:
            give_item(hero, entry.id, 1)

on_ready():
    applied_movement_speed = 0
    apply_player_attributes()
    configure_view()
    draw_interface()

on_loaded():
    state = get_save_state()
    if state.schema != 1:
        script_error("Unsupported foundation save schema")
    hero = get_entity("player")
    avatar = get_entity("player_visual")
    blade = get_entity("player_weapon")
    if hero == null or avatar == null or blade == null:
        script_error("Saved player is incomplete")
    applied_movement_speed = 0
    apply_player_attributes()
    set_gravity_factor(hero, get_definition("Entities", Actor).gravity)
    current = equipped_instance(hero, "weapon")
    if current != null:
        state.weapon = item_instance(hero, current).item
    equip(state.weapon)
    for instance in item_instances(hero):
        if instance.slot != "" and instance.slot != "weapon":
            armor_spec = get_definition("Items", instance.item)
            if record_has(armor_spec, "armor_slot"):
                equip_armor_visual(armor_spec)
    if not record_has(state, "stored_drops"):
        state.stored_drops = []

on_saved():
    notice = "Mundo salvo."

on_before_save():
    set_save_state(state)

on_fixed_update(delta):
    if hero == null:
        return
    combat_tick(delta)
    if health(hero) <= 0 or hero.position.y < -10:
        state.gold = math_floor(state.gold * (1 - get_definition("Rules", Gameplay).death_gold_loss))
        heal(hero, maximum_health(hero))
        teleport(hero, vec3(hero.position.x, terrain_height(hero.position.x, hero.position.z) + 1.1, hero.position.z))
        notice = "Voce se recuperou. Parte do ouro foi perdida."
    encounter_timer = encounter_timer - delta
    if encounter_timer <= 0:
        update_encounters()
        encounter_timer = 0.8
    for drop in get_entities_with(GroundDrop):
        if gap(hero.position, drop.position) < 1.5:
            data = get_component(drop, GroundDrop)
            give_item(hero, data.item, 1)
            play_sound("foundation:pickup")
            notice = "Coletou: " + get_definition("Items", data.item).name
            destroy(drop)

on_update(delta):
    if hero == null:
        return
    if action_pressed("Inventory"):
        inventory_open = not inventory_open
        attack_time = 0
        winding = false
        combo_time = 0
    movement_enabled(not inventory_open)
    if inventory_open:
        entries = inventory_entries(hero)
        count = list_count(entries)
        if count > 0:
            selected = selected % count
            if action_pressed("Equip"):
                selected = (selected + 1) % count
            if action_pressed("Interact"):
                equip(list_at(entries, selected).id)
    else:
        if action_pressed("Jump"):
            jump(get_attribute(hero, "foundation:jump_speed"))
        target = aim_position()
        if attack_time <= 0 and target != null:
            facing = heading(hero.position, target)
        if action_pressed("Attack") and attack_time <= 0:
            begin_attack()
    set_rotation(avatar, vec3(0, math_atan2(-facing.x, -facing.z), 0))
    walk_time = walk_time + delta
    if not is_studio_model(blade):
        swing = 0
        if attack_time > 0:
            swing = -0.9
            if not winding:
                swing = 0.8
        set_rotation(blade, vec3(swing, 0, -0.35))
    animate_player()
    hud_timer = hud_timer - delta
    if hud_timer <= 0:
        draw_interface()
        hud_timer = 0.1
