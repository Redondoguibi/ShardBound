func map_marker(x, z, label, active):
    marker = record()
    marker.x = x
    marker.z = z
    marker.label = label
    marker.active = active
    return marker

func refresh_hud():
    region = "ABRIGO"
    if hero.position.z >= 7:
        region = "FLORESTA"
    spec = weapon_definition()
    text = "SHARDBOUND  |  " + region + "  |  Vida " + number_text(health(hero)) + "  |  Ouro " + number_text(state.gold)
    text = text + "\n" + spec.name + "  Nv." + number_text(spec.level) + "  " + spec.rarity
    text = text + "\nWASD mover   Espaco saltar   Mouse atacar   M mapa   I inventario"
    text = text + "\nF5 salvar   F9 carregar   Q trocar arma   Esc sair"
    if inventory_open:
        text = text + "\nInventario: ferro " + number_text(item_count(hero, IronSword)) + " | floresta " + number_text(item_count(hero, ForestSword))
    else:
        if map_open:
            text = text + "\nViagem: 1 abrigo / 2 floresta / 3 clareira. Bloqueio: " + number_text(math_floor(state.combat + 0.999)) + "s"
        else:
            text = text + "\n" + notice
    text = text + "\nExplore livremente. O abrigo recupera sua vida."
    markers = [map_marker(hero.position.x, hero.position.z, "Voce", true), map_marker(3, 0, "1 Abrigo", state.hub), map_marker(-3, 20, "2 Floresta", state.forest), map_marker(3, 36, "3 Clareira", state.grove)]
    show_ui(text, map_open, markers)

func handle_input():
    if action_pressed("Map"):
        map_open = not map_open
        inventory_open = false
    if action_pressed("Inventory"):
        inventory_open = not inventory_open
        map_open = false
    if action_pressed("Jump") and not map_open:
        jump(get_definition("Entities", Actor).jump)
    if action_pressed("Attack") and not map_open and not inventory_open:
        begin_attack()
    if action_pressed("Equip") and state.attack == 0:
        if state.equipped == "adventure:IronSword" and item_count(hero, ForestSword) > 0:
            state.equipped = "adventure:ForestSword"
            set_tint(blade, vec3(0.55, 1, 0.7))
        else:
            state.equipped = "adventure:IronSword"
            set_tint(blade, vec3(1, 1, 1))
    if map_open:
        if action_pressed("Travel1"):
            travel("hub_waystone", state.hub)
        if action_pressed("Travel2"):
            travel("forest_waystone", state.forest)
        if action_pressed("Travel3"):
            travel("grove_waystone", state.grove)
    if action_pressed("Save"):
        save_game("adventure")
    if action_pressed("Load"):
        load_game("adventure")
