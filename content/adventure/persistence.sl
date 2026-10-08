@Events
on_validate_load(saved):
    if saved.adventure.schema != 1:
        script_error("Unsupported adventure save schema")
    data = saved.adventure
    if record_has(data, "equipment_schema"):
        if data.equipment_schema != 1 or list_count(data.items) > 256:
            script_error("Unsupported or oversized equipment state")
        last_uid = 0
        equipped_found = data.equipped_uid == 0
        for item in data.items:
            validate_item(item)
            if item.uid <= last_uid:
                script_error("Equipment identities must be unique and increasing")
            last_uid = item.uid
            if item.uid == data.equipped_uid:
                equipped_found = true
        if not equipped_found:
            script_error("Equipped instance is absent from saved inventory")
        if data.next_item_uid <= last_uid or data.next_item_uid != math_floor(data.next_item_uid) or data.next_item_uid > 9007199254740991:
            script_error("Invalid next equipment identity")
    if record_has(data, "discoveries"):
        if list_count(data.discoveries) > 256:
            script_error("Too many saved discoveries")
        seen = []
        for flag in data.discoveries:
            if text_length(flag.id) == 0 or text_length(flag.id) > 128 or (flag.active != true and flag.active != false):
                script_error("Invalid saved discovery")
            if list_contains(seen, flag.id):
                script_error("Duplicate saved waystone discovery")
            list_append(seen, flag.id)
    if record_has(data, "built_regions"):
        if list_count(data.built_regions) > 256:
            script_error("Too many saved regions")
        for id in data.built_regions:
            if text_length(id) == 0 or text_length(id) > 128:
                script_error("Invalid saved region identity")
    if record_has(data, "encounters"):
        if list_count(data.encounters) > 256:
            script_error("Too many saved encounters")
        seen = []
        for key in data.encounters:
            if text_length(key) == 0 or text_length(key) > 128 or list_contains(seen, key):
                script_error("Invalid or duplicate saved encounter")
            list_append(seen, key)

on_before_save():
    ensure_equipment()
    state.hero_key = entity_key(hero)
    state.avatar_key = entity_key(avatar)
    state.blade_key = entity_key(blade)
    set_save_state(state)

on_loaded():
    restored = get_save_state()
    state = restored
    hero = get_entity_by_key(state.hero_key)
    avatar = get_entity_by_key(state.avatar_key)
    blade = get_entity_by_key(state.blade_key)
    if not record_has(state, "equipment_schema"):
        state.equipment_schema = 1
        state.items = []
        state.next_item_uid = 1
        state.equipped_uid = 0
        sync_equipment()
        for item in state.items:
            if item.definition == state.equipped:
                state.equipped_uid = item.uid
    if not record_has(state, "discoveries"):
        state.discoveries = []
        discovery("adventure:Hub").active = state.hub
        discovery("adventure:Forest").active = state.forest
        discovery("adventure:Grove").active = state.grove
    if not record_has(state, "built_regions"):
        state.built_regions = ["adventure:Hub", "adventure:Forest"]
    if not record_has(state, "encounters"):
        state.encounters = []
    sync_world_content()
    build_combat_visuals()
    configure_presentation()
    paused = false
    title_open = false
    pause_game(false)
    damage_labels = []
    attack_buffer = 0
    ensure_equipment()
    if equipped_item() != null:
        spec = weapon_definition()
        set_model(blade, spec.model)
        set_scale(blade, spec.visual_scale)
        set_tint(blade, spec.tint)
    inventory_filter = 0
    selected_item = 0
    selected_destination = 0
    map_open = false
    inventory_open = false
    notice = "Jogo carregado."
    keyboard_movement_only(true)
    refresh_hud()

on_saved():
    notice = "Jogo salvo."
    refresh_hud()

on_save_diagnostic(loading, message):
    notice = "Não foi possível salvar. Tente novamente."
    if loading:
        notice = "Não foi possível carregar. Seu progresso atual foi mantido."
    refresh_hud()
