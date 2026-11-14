func configure_view():
    keyboard_movement_only(true)
    set_camera_offset(vec3(7, 11, -10))
    env = record()
    env.sky = vec3(0.40, 0.55, 0.63)
    env.sun = vec3(1.05, 0.98, 0.82)
    env.ambient = vec3(0.48, 0.53, 0.51)
    env.fog_start = 32
    env.fog_end = 70
    env.shadows = true
    set_environment(env)

func panel(x, y, w, h):
    c = record()
    c.kind = "rect"
    c.x = x
    c.y = y
    c.w = w
    c.h = h
    c.color = vec3(0.05, 0.09, 0.08)
    c.alpha = 0.93
    list_append(canvas, c)

func label(value, x, y, w, size=18):
    c = record()
    c.kind = "text"
    c.x = x
    c.y = y
    c.w = w
    c.h = size + 12
    c.size = size
    c.text = value
    c.align = "left"
    c.color = vec3(0.91, 0.94, 0.86)
    c.alpha = 1
    list_append(canvas, c)

func draw_interface():
    canvas = []
    if hero == null:
        return
    origin = world_origin()
    panel(22, 20, 500, 118)
    label("Vida " + number_text(health(hero)) + " / " + number_text(maximum_health(hero)) + "    Ouro " + number_text(state.gold), 40, 32, 465, 22)
    label(get_definition("Items", state.weapon).name, 40, 66, 465)
    label("X " + number_text(origin.x + hero.position.x) + "   Z " + number_text(origin.z + hero.position.z), 40, 98, 465, 16)
    panel(22, 622, 1236, 78)
    label(notice, 40, 632, 1190)
    label("WASD mover | Espaco saltar | Clique atacar | I inventario | F5 salvar | Esc pausa", 40, 664, 1190, 16)
    if inventory_open:
        panel(270, 165, 740, 420)
        label("Equipamentos", 300, 188, 680, 28)
        entries = inventory_entries(hero)
        first_visible = 0
        if selected >= 8:
            first_visible = selected - 7
        index = 0
        for entry in entries:
            if index >= first_visible and index < first_visible + 8:
                prefix = "  "
                if index == selected:
                    prefix = "> "
                spec = get_definition("Items", entry.id)
                detail = ""
                if record_has(spec, "armor_slot"):
                    detail = "  protecao " + number_text(spec.protection)
                    if record_has(spec, "movement_bonus") and spec.movement_bonus != 0:
                        detail = detail + "  vel. " + number_text(spec.movement_bonus)
                    if record_has(spec, "health_bonus") and spec.health_bonus != 0:
                        detail = detail + "  vida " + number_text(spec.health_bonus)
                else:
                    if record_has(spec, "damage"):
                        detail = "  dano " + number_text(spec.damage)
                label(prefix + spec.name + "  x" + number_text(entry.count) + detail, 300, 245 + (index - first_visible) * 32, 680)
            index = index + 1
        label("Q seleciona | E equipa | I fecha", 300, 538, 680, 18)
    show_canvas(canvas)
