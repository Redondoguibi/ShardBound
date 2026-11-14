func update_encounters():
    origin = world_origin()
    gx = math_floor((origin.x + hero.position.x) / 16)
    gz = math_floor((origin.z + hero.position.z) / 16)
    radius = get_definition("Rules", Gameplay).encounter_radius
    for foe in get_entities_with(Brain):
        if gap(foe.position, hero.position) > get_definition("Rules", Gameplay).despawn_distance:
            brain = get_component(foe, Brain)
            destroy(get_entity_by_key(brain.visual))
            destroy(foe)
    for drop in get_entities_with(GroundDrop):
        if gap(drop.position, hero.position) > 48:
            stored = record()
            stored.x = origin.x + drop.position.x
            stored.z = origin.z + drop.position.z
            stored.item = get_component(drop, GroundDrop).item
            list_append(state.stored_drops, stored)
            destroy(drop)
    remaining = []
    for stored in state.stored_drops:
        dx = stored.x - origin.x - hero.position.x
        dz = stored.z - origin.z - hero.position.z
        if dx * dx + dz * dz < 36 * 36:
            px = stored.x - origin.x
            pz = stored.z - origin.z
            drop = spawn(Drop, vec3(px, terrain_height(px, pz) + 0.2, pz))
            set_scale(drop, vec3(0.035, 0.035, 0.035))
            add_component(drop, GroundDrop)
            loot = get_component(drop, GroundDrop)
            loot.item = stored.item
            set_component(drop, GroundDrop, loot)
        else:
            list_append(remaining, stored)
    state.stored_drops = remaining
    for entry in get_definitions("Encounters"):
        spec = entry.data
        if spec.generator == world_generator():
            for dx in number_range(radius * 2 + 1):
                for dz in number_range(radius * 2 + 1):
                    x = gx + dx - radius
                    z = gz + dz - radius
                    key = entry.id + ":" + number_text(x) + ":" + number_text(z)
                    exists = list_contains(state.defeated, key)
                    for foe in get_entities_with(Brain):
                        if get_component(foe, Brain).key == key:
                            exists = true
                    if (x != 0 or z != 0) and not exists and world_random(x, z, spec.salt) < spec.chance:
                        px = x * 16 + 5 + math_floor(world_random(x, z, spec.salt + 1) * 6) - origin.x
                        pz = z * 16 + 5 + math_floor(world_random(x, z, spec.salt + 2) * 6) - origin.z
                        matches_biome = true
                        if record_has(spec, "biomes"):
                            matches_biome = list_contains(spec.biomes, terrain_biome(px, pz))
                        if matches_biome and terrain_clear(px, pz):
                            stats = get_definition("Entities", spec.enemy)
                            foe = spawn(spec.enemy, vec3(px, terrain_height(px, pz) + stats.body.y + 0.1, pz))
                            add_box(foe, stats.body, true)
                            set_health(foe, stats.health)
                            look = spawn(stats.visual, vec3(0, -stats.body.y, 0))
                            set_parent(look, foe)
                            add_component(foe, Brain)
                            brain = get_component(foe, Brain)
                            brain.key = key
                            brain.visual = entity_key(look)
                            set_component(foe, Brain, brain)
@Events
on_developer_spawn(foe):
    stats = get_definition("Entities", entity_definition(foe))
    if record_has(stats, "visual") and record_has(stats, "body") and record_has(stats, "health"):
        add_box(foe, stats.body, true)
        set_health(foe, stats.health)
        look = spawn(stats.visual, vec3(0, -stats.body.y, 0))
        set_parent(look, foe)
        add_component(foe, Brain)
        brain = get_component(foe, Brain)
        brain.key = "developer:" + entity_key(foe)
        brain.visual = entity_key(look)
        set_component(foe, Brain, brain)
