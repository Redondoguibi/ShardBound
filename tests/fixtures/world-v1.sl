func block_at(where, scale, color, solid):
    piece = spawn(Block, where)
    set_scale(piece, scale)
    set_tint(piece, color)
    if solid:
        add_box(piece, scale, false)
    return piece

func shelter(x, z):
    wall = block_at(vec3(x, 1.3, z), vec3(2, 1.3, 1.7), vec3(0.58, 0.45, 0.29), true)
    set_model(wall, "core:plaster")
    block_at(vec3(x, 2.9, z), vec3(2.4, 0.3, 2.1), vec3(0.26, 0.38, 0.42), false)
    block_at(vec3(x, 0.85, z - 1.72), vec3(0.5, 0.85, 0.03), vec3(0.18, 0.14, 0.12), false)
    block_at(vec3(x + 1.1, 1.5, z - 1.73), vec3(0.35, 0.35, 0.04), vec3(0.95, 0.8, 0.44), false)

func stone_at(label, where):
    stone = spawn(Waystone, where)
    stone.name = label
    set_scale(stone, vec3(0.055, 0.055, 0.055))
    set_tint(stone, vec3(0.5, 0.55, 0.6))
    return stone

func enemy_at(x, z, definition="adventure:Enemy"):
    stats = get_definition("Entities", definition)
    if stats.body.x <= 0 or stats.body.y <= 0 or stats.body.z <= 0 or stats.max_health <= 0 or stats.speed < 0 or stats.damage < 0 or stats.windup <= 0 or stats.recovery <= 0 or stats.respawn <= 0 or stats.stop <= 0 or stats.trigger < stats.stop or stats.reach < stats.trigger or stats.aggro < stats.reach or stats.reward < 0:
        script_error("Invalid enemy combat or body attributes")
    foe = spawn(definition, vec3(x, stats.body.y, z))
    add_box(foe, stats.body, true)
    set_health(foe, stats.max_health)
    look = spawn(stats.visual, vec3(0, -stats.body.y, 0))
    set_parent(look, foe)
    set_scale(look, stats.visual_scale)
    set_tint(look, stats.tint)
    add_component(foe, Sentinel)
    brain = get_component(foe, Sentinel)
    brain.visual = entity_key(look)
    brain.home = vec3(x, stats.body.y, z)
    set_component(foe, Sentinel, brain)
    return foe

func sync_encounters(region):
    if not record_has(region.data, "encounters"):
        return
    for entry in region.data.encounters:
        if not list_contains(state.encounters, entry.key):
            enemy_at(entry.position.x, entry.position.z, entry.definition)
            list_append(state.encounters, entry.key)

func build_region(region):
    spec = region.data
    center = vec3((spec.min.x + spec.max.x) / 2, -0.5, (spec.min.z + spec.max.z) / 2)
    half = vec3((spec.max.x - spec.min.x) / 2, 0.5, (spec.max.z - spec.min.z) / 2)
    block_at(center, half, spec.color, true)
    for where in spec.shelters:
        shelter(where.x, where.z)
    for where in spec.trees:
        tree = spawn(Tree, where)
        set_scale(tree, vec3(0.038, 0.038, 0.038))
        trunk = block_at(vec3(where.x, where.y + 1, where.z), vec3(0.3, 1, 0.3), vec3(0.35, 0.27, 0.19), true)
        trunk.active = false
    for where in spec.enemies:
        enemy_at(where.x, where.z)
    list_append(state.built_regions, region.id)

func sync_world_content():
    validate_world_content()
    for region in region_catalog:
        built = false
        for id in state.built_regions:
            if id == region.id:
                built = true
        if not built:
            build_region(region)
        sync_encounters(region)
    for entry in destination_list():
        if get_entity(entry.data.label) == null:
            stone_at(entry.data.label, entry.data.position)

func build_world():
    sync_world_content()
    road = block_at(vec3(0, 0.016, 17), vec3(1.8, 0.01, 21), vec3(0.53, 0.46, 0.32), false)
    road.name = "adventure_legacy_road"
    road.active = false
    for x in [-4, 4]:
        block_at(vec3(x, 0.25, 12), vec3(0.8, 0.25, 0.8), vec3(0.55, 0.6, 0.59), true)
    hero = spawn(Actor, get_definition("Worlds", "adventure:Main").start)
    hero.name = "player"
    add_box(hero, vec3(0.4, 1, 0.3), true)
    stats = get_definition("Entities", Actor)
    set_health(hero, stats.max_health)
    set_player(hero, stats.speed)
    avatar = spawn(Explorer, vec3(0, -1, 0))
    avatar.name = "player_visual"
    set_parent(avatar, hero)
    blade = spawn(Blade, vec3(0.375, -0.6, 0))
    blade.name = "player_sword"
    set_parent(blade, avatar)
    set_scale(blade, vec3(0.075, 0.075, 0.075))
    give_item(hero, IronSword, 1)
    build_combat_visuals()
