@Events
on_validate_load(saved):
    if saved.adventure.schema != 1:
        script_error("Unsupported adventure save schema")

on_before_save():
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
    map_open = false
    inventory_open = false
    notice = "Jogo carregado."
    keyboard_movement_only(true)
    refresh_hud()

on_saved():
    notice = "Jogo salvo. F9 para carregar."
    refresh_hud()

on_save_diagnostic(loading, message):
    notice = "Aviso ao acessar o save. Detalhes no log."
    refresh_hud()
