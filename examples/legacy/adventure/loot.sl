? World loot is persistent entity data; no random table or core-specific rules.
func drop_item(definition, where):
    loot = spawn(LootVisual, vec3(where.x, where.y + 0.2, where.z))
    set_scale(loot, vec3(0.035, 0.035, 0.035))
    set_tint(loot, vec3(0.65, 1, 0.75))
    if definition_exists("Items", definition):
        spec = get_definition("Items", definition)
        if record_has(spec, "model"):
            set_model(loot, spec.model)
        if record_has(spec, "tint"):
            set_tint(loot, spec.tint)
    add_component(loot, GroundLoot)
    data = get_component(loot, GroundLoot)
    data.item = definition
    set_component(loot, GroundLoot, data)
    return loot

func collect_loot():
    if health(hero) <= 0:
        return
    for loot in get_entities_with(GroundLoot):
        data = get_component(loot, GroundLoot)
        if distance_xz(hero.position, loot.position) <= 2.5 and definition_exists("Items", data.item):
            spec = get_definition("Items", data.item)
            weapon = record_has(spec, "slot") and spec.slot == "weapon"
            sync_equipment()
            if weapon and list_count(state.items) >= 256:
                notice = "Inventario cheio. O item continua no chao."
            else:
                ? Validate hooks before granting the count and consuming the world drop.
                if weapon:
                    create_equipment(data.item)
                give_item(hero, data.item, 1)
                play_sound("core:pickup")
                destroy(loot)
                notice = "Item coletado: " + spec.name
                emit_event("adventure:collected", [data.item])
