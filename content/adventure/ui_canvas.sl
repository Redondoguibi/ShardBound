@Globals
canvas = []
paused = false
title_open = false
menu_selected = 0
volume = 0.75
last_notice = ""
notice_time = 5
damage_labels = []
attack_buffer = 0
ink = vec3(0.90, 0.92, 0.85)
muted = vec3(0.55, 0.64, 0.62)
gold_color = vec3(0.88, 0.70, 0.36)
panel_color = vec3(0.055, 0.10, 0.11)
accent = vec3(0.38, 0.78, 0.68)

func minimum(a, b):
    if a < b:
        return a
    return b

func shape(kind, x, y, w, h, color, alpha=1):
    command = record()
    command.kind = kind
    command.x = x
    command.y = y
    command.w = w
    command.h = h
    command.color = color
    command.alpha = alpha
    list_append(canvas, command)

func label(text, x, y, w, size=18, color=ink, align="left"):
    command = record()
    command.kind = "text"
    command.text = text
    command.x = x
    command.y = y
    command.w = w
    command.h = size + 12
    command.size = size
    command.color = color
    command.alpha = 1
    command.align = align
    list_append(canvas, command)

func card(x, y, w, h):
    shape("rect", x + 5, y + 7, w, h, vec3(0, 0, 0), 0.2)
    shape("rect", x, y, w, h, panel_color, 0.96)
    shape("rect", x, y, w, 2, vec3(0.30, 0.39, 0.37))

func inside(x, y, w, h):
    p = pointer_position()
    return p.valid and p.x >= x and p.y >= y and p.x < x + w and p.y < y + h

func button(text, x, y, w, selected=false):
    color = vec3(0.10, 0.17, 0.18)
    if selected or inside(x, y, w, 48):
        color = vec3(0.20, 0.32, 0.30)
    shape("rect", x, y, w, 48, color)
    if selected:
        shape("rect", x, y, 3, 48, gold_color)
    label(text, x + 16, y + 7, w - 32, 20)

func rarity_color(rarity):
    if rarity == "Incomum":
        return accent
    return gold_color

func draw_hud():
    ui_spec = weapon_definition()
    region = current_region()
    name = "TERRAS DESCONHECIDAS"
    subtitle = "EXPLORAÇÃO LIVRE"
    if region != null:
        name = region.name
        if region.safe:
            subtitle = "ZONA SEGURA"
    shape("rect", 488, 18, 304, 67, panel_color, 0.82)
    label(name, 498, 22, 284, 24, ink, "center")
    label(subtitle, 498, 55, 284, 12, accent, "center")
    shape("diamond", 620, 85, 40, 5, gold_color)
    shape("rect", 1110, 26, 138, 42, panel_color, 0.86)
    shape("diamond", 1125, 40, 14, 14, gold_color)
    label(number_text(state.gold), 1148, 31, 84, 22, gold_color, "right")
    card(28, 594, 286, 98)
    label("VIDA", 44, 604, 120, 13, muted)
    label(number_text(health(hero)) + " / " + number_text(maximum_health(hero)), 170, 603, 126, 16, ink, "right")
    shape("rect", 44, 635, 254, 13, vec3(0.12, 0.18, 0.18))
    shape("rect", 44, 635, 254 * health(hero) / maximum_health(hero), 13, vec3(0.69, 0.26, 0.22))
    shape("rect", 44, 635, 254 * health(hero) / maximum_health(hero), 2, vec3(0.95, 0.53, 0.37))
    if equipped_item() == null:
        label("Sem arma equipada", 44, 655, 254, 16, muted)
    else:
        label(ui_spec.name + "  ·  Nv. " + number_text(ui_spec.level), 44, 655, 254, 16, rarity_color(ui_spec.rarity))
    shape("rect", 908, 636, 340, 56, panel_color, 0.82)
    label("I  Equipamento     M  Mapa     Esc  Pausa", 924, 640, 312, 15)
    label("WASD  Mover     Espaço  Saltar     Clique  Atacar", 924, 666, 310, 12, muted)
    if state.combat > 0:
        label("EM COMBATE  ·  " + number_text(math_floor(state.combat + 0.999)) + "s", 512, 650, 256, 14, gold_color, "center")
    if notice_time > 0 and text_length(notice) > 0:
        shape("rect", 336, 550, 608, 42, panel_color, 0.87)
        label(notice, 350, 556, 580, 17, ink, "center")
    for entry in damage_labels:
        p = screen_position(entry.position)
        if p.visible:
            label(entry.text, p.x - 60, p.y - 48 - (0.7 - entry.life) * 50, 120, 28, entry.color, "center")
    for foe in get_entities_with(Sentinel):
        if health(foe) > 0 and health(foe) < maximum_health(foe) and distance_xz(hero.position, foe.position) < 10:
            p = screen_position(vec3(foe.position.x, foe.position.y + 1.25, foe.position.z))
            if p.visible:
                shape("rect", p.x - 28, p.y - 6, 56, 5, panel_color)
                shape("rect", p.x - 28, p.y - 6, 56 * health(foe) / maximum_health(foe), 5, vec3(0.82, 0.39, 0.29))

func draw_map():
    shape("rect", 0, 0, 1280, 720, vec3(0.02, 0.05, 0.06), 0.65)
    card(150, 70, 980, 580)
    label("CAMINHOS DESCOBERTOS", 184, 91, 720, 30)
    label("M / Esc  Fechar", 925, 101, 169, 15, muted, "right")
    label("Waystones", 774, 161, 300, 22)
    shape("rect", 186, 158, 540, 426, vec3(0.12, 0.21, 0.20))
    destinations = destination_list()
    min_x = -18
    max_x = 18
    min_z = -14
    max_z = 42
    for entry in destinations:
        min_x = minimum(min_x, entry.data.position.x - 4)
        max_x = maximum(max_x, entry.data.position.x + 4)
        min_z = minimum(min_z, entry.data.position.z - 4)
        max_z = maximum(max_z, entry.data.position.z + 4)
    for region in region_catalog:
        min_x = minimum(min_x, region.data.min.x)
        max_x = maximum(max_x, region.data.max.x)
        min_z = minimum(min_z, region.data.min.z)
        max_z = maximum(max_z, region.data.max.z)
    for region in region_catalog:
        ui_spec = region.data
        color = vec3(0.16, 0.29, 0.23)
        if ui_spec.safe:
            color = vec3(0.31, 0.33, 0.25)
        x = 200 + (ui_spec.min.x - min_x) / (max_x - min_x) * 510
        y = 570 - (ui_spec.max.z - min_z) / (max_z - min_z) * 398
        shape("rect", x, y, (ui_spec.max.x - ui_spec.min.x) / (max_x - min_x) * 510, (ui_spec.max.z - ui_spec.min.z) / (max_z - min_z) * 398, color)
    ? Small symbols use the same public region locations as the playable world.
    if list_count(region_catalog) < 24:
        for region in region_catalog:
            for index in number_range(minimum(32, list_count(region.data.trees))):
                where = list_at(region.data.trees, index)
                x = 200 + (where.x - min_x) / (max_x - min_x) * 510
                y = 570 - (where.z - min_z) / (max_z - min_z) * 398
                shape("diamond", x - 6, y - 9, 12, 18, vec3(0.22, 0.39, 0.29))
            for index in number_range(minimum(16, list_count(region.data.shelters))):
                where = list_at(region.data.shelters, index)
                x = 200 + (where.x - min_x) / (max_x - min_x) * 510
                y = 570 - (where.z - min_z) / (max_z - min_z) * 398
                shape("rect", x - 8, y - 6, 16, 12, vec3(0.57, 0.54, 0.38))
    if definition_exists("Regions", "adventure:Hub") and definition_exists("Regions", "adventure:Forest"):
        for index in number_range(60):
            z = -4 + index * 0.7
            x = math_sin(index * 0.15) * 1.2
            mx = 200 + (x - min_x) / (max_x - min_x) * 510
            my = 570 - (z - min_z) / (max_z - min_z) * 398
            shape("rect", mx - 2, my - 3, 4, 7, vec3(0.50, 0.49, 0.31))
    label("N", 662, 176, 28, 14, muted, "center")
    shape("diamond", 672, 198, 8, 18, muted)
    for index in number_range(list_count(destinations)):
        entry = list_at(destinations, index)
        x = 200 + (entry.data.position.x - min_x) / (max_x - min_x) * 510
        y = 570 - (entry.data.position.z - min_z) / (max_z - min_z) * 398
        color = muted
        if discovered(entry.id):
            color = accent
        shape("diamond", x - 9, y - 9, 18, 18, color)
        if index == selected_destination:
            shape("diamond", x - 4, y - 4, 8, 8, ink)
            label(entry.data.name, maximum(202, minimum(548, x + 14)), y - 14, 158, 15, ink)
        if math_floor(index / 5) == math_floor(selected_destination / 5):
            status = "  ·  Não descoberta"
            if discovered(entry.id):
                status = "  ·  Ativa"
            button(entry.data.name + status, 774, 211 + index % 5 * 62, 320, index == selected_destination)
    x = 200 + (hero.position.x - min_x) / (max_x - min_x) * 510
    y = 570 - (hero.position.z - min_z) / (max_z - min_z) * 398
    shape("diamond", x - 5, y - 5, 10, 10, gold_color)
    label("VOCÊ", x - 35, y + 8, 70, 12, gold_color, "center")
    text = "E  Viajar gratuitamente"
    if state.combat > 0:
        text = "Em combate  ·  Aguarde " + number_text(math_floor(state.combat + 0.999)) + "s"
    label(text, 774, 535, 320, 18, gold_color)
    button("Q  Próxima waystone  →", 774, 579, 320)
    label("Aproxime-se das pedras para ativá-las.", 186, 599, 540, 15, muted)

func menu_options():
    if title_open:
        return ["Explorar", "Carregar jogo", "Sair"]
    return ["Continuar", "Salvar jogo", "Carregar jogo", "Volume: " + number_text(volume * 100) + "%", "Sair sem salvar"]

func draw_menu():
    shape("rect", 0, 0, 1280, 720, vec3(0.02, 0.055, 0.065), 0.7)
    heading = "PAUSA"
    if title_open:
        heading = "SHARDBOUND"
    shape("diamond", 620, 79, 40, 55, gold_color)
    label(heading, 240, 148, 800, 56, ink, "center")
    if title_open:
        label("Um mundo para explorar. Um mundo para transformar.", 240, 219, 800, 19, muted, "center")
    options = menu_options()
    for index in number_range(list_count(options)):
        button(list_at(options, index), 455, 285 + index * 59, 370, index == menu_selected)
    label("W / S  Selecionar     E  Confirmar     Mouse  Escolher", 240, 638, 800, 15, muted, "center")
    if notice_time > 0 and notice != "Explore a floresta ao norte.":
        label(notice, 240, 672, 800, 16, gold_color, "center")

func activate_menu(index):
    if index == 0:
        paused = false
        title_open = false
        pause_game(false)
    else:
        if title_open:
            if index == 1:
                load_game("adventure")
            if index == 2:
                quit_game()
        else:
            if index == 1:
                save_game("adventure")
            if index == 2:
                load_game("adventure")
            if index == 3:
                volume = volume + 0.25
                if volume > 1:
                    volume = 0
                audio_volume(volume)
            if index == 4:
                quit_game()
    refresh_hud()

func menu_input():
    count = list_count(menu_options())
    if action_pressed("MoveBackward"):
        menu_selected = (menu_selected + 1) % count
    if action_pressed("MoveForward"):
        menu_selected = (menu_selected + count - 1) % count
    if action_pressed("Interact"):
        activate_menu(menu_selected)
    if action_pressed("Attack"):
        for index in number_range(count):
            if inside(455, 285 + index * 59, 370, 48):
                activate_menu(index)

func canvas_input():
    if not action_pressed("Attack"):
        return
    if inventory_open:
        inventory_click()
    if map_open:
        destinations = destination_list()
        for index in number_range(list_count(destinations)):
            if math_floor(index / 5) == math_floor(selected_destination / 5) and inside(774, 211 + index % 5 * 62, 320, 48):
                selected_destination = index
                travel_destination(list_at(destinations, index).id)
        if inside(774, 579, 320, 48) and list_count(destinations) > 0:
            selected_destination = (selected_destination + 1) % list_count(destinations)

func presentation_tick(delta):
    if notice != last_notice:
        last_notice = notice
        notice_time = 5
    notice_time = countdown(notice_time, delta)
    if paused:
        return
    live = []
    for entry in damage_labels:
        entry.life = countdown(entry.life, delta)
        if entry.life > 0:
            list_append(live, entry)
    damage_labels = live

func floating_damage(where, amount, critical=false):
    if list_count(damage_labels) >= 16:
        return
    entry = record()
    entry.position = where
    entry.life = 0.7
    entry.text = number_text(amount)
    entry.color = ink
    if critical:
        entry.text = entry.text + "!"
        entry.color = gold_color
    list_append(damage_labels, entry)

func refresh_hud():
    canvas = []
    if not paused and not inventory_open and not map_open:
        draw_hud()
    if inventory_open:
        draw_inventory()
    if map_open:
        draw_map()
    if paused:
        draw_menu()
    show_canvas(canvas)
