? Inventory presentation uses real instance identities and attributes; it never grants items.
@Globals
inventory_filter = 0
inventory_page_size = 12

func stat_number(value):
    scaled = math_floor(value * 100 + 0.5)
    whole = math_floor(scaled / 100)
    fraction = scaled % 100
    if fraction == 0:
        return number_text(whole)
    prefix = ""
    if fraction < 10:
        prefix = "0"
    return number_text(whole) + "," + prefix + number_text(fraction)

func item_presentation(row):
    info = record()
    info.available = false
    info.equipped = false
    info.name = ""
    info.rarity = ""
    info.level = 0
    info.damage = 0
    info.reach = 0
    info.windup = 0
    info.recovery = 0
    info.pattern = "slash"
    if row.equipment:
        info.name = row.item.definition
        info.level = row.item.level
        info.rarity = row.item.rarity
        info.equipped = row.item.uid == state.equipped_uid
        if definition_exists("Items", row.item.definition):
            info = data_merge(info, data_merge(get_definition("Items", row.item.definition), row.item.attributes))
            info.level = row.item.level
            info.rarity = row.item.rarity
            info.available = true
    else:
        info.name = row.stack.id
        if definition_exists("Items", row.stack.id):
            stack_spec = get_definition("Items", row.stack.id)
            if record_has(stack_spec, "name"):
                info.name = stack_spec.name
    return info

func weapon_emblem(x, y, scale, info):
    width = 10
    length = 43
    if record_has(info, "visual_scale"):
        width = minimum(16, maximum(6, info.visual_scale.x * 200))
        length = minimum(50, maximum(26, info.visual_scale.y * 860))
    shape("diamond", x - width * scale, y, width * 2 * scale, length * scale, vec3(0.63, 0.77, 0.75))
    shape("rect", x - scale * 1.5, y + 10 * scale, 3 * scale, (length - 7) * scale, vec3(0.88, 0.92, 0.84))
    shape("rect", x - 13 * scale, y + length * scale, 26 * scale, 3 * scale, rarity_color(info.rarity))
    shape("rect", x - 3 * scale, y + (length + 3) * scale, 6 * scale, 12 * scale, vec3(0.40, 0.25, 0.14))

func inventory_tab(text, x, index):
    color = vec3(0.08, 0.14, 0.15)
    if index == inventory_filter or inside(x, 144, 182, 38):
        color = vec3(0.16, 0.26, 0.25)
    shape("rect", x, 144, 182, 38, color)
    label(text, x + 12, 150, 158, 16)
    if index == inventory_filter:
        shape("rect", x, 180, 182, 2, accent)

func draw_stat(name, value, baseline, y, higher_better=true, suffix=""):
    label(name, 750, y, 164, 15, muted)
    label(stat_number(value) + suffix, 930, y - 3, 112, 22, ink, "right")
    if equipped_item() != null:
        diff = value - baseline
        if diff > 0.001 or diff < -0.001:
            good = diff > 0
            if not higher_better:
                good = not good
            color = vec3(0.92, 0.48, 0.37)
            if good:
                color = accent
            prefix = "−"
            if diff > 0:
                prefix = "+"
            label(prefix + stat_number(maximum(diff, -diff)), 1048, y, 100, 16, color, "right")

func draw_inventory():
    shape("rect", 0, 0, 1280, 720, vec3(0.02, 0.05, 0.06), 0.70)
    card(64, 44, 1152, 632)
    label("INVENTÁRIO", 88, 64, 650, 34)
    label("I / Esc  Fechar", 984, 72, 208, 15, muted, "right")
    label("Selecione para comparar. Equipe quando decidir.", 90, 107, 700, 15, muted)
    inventory_tab("Todos", 88, 0)
    inventory_tab("Armas", 282, 1)
    inventory_tab("Outros itens", 476, 2)
    rows = inventory_rows()
    size = list_count(rows)
    if selected_item >= size:
        selected_item = maximum(0, size - 1)
    page = math_floor(selected_item / inventory_page_size)
    for slot in number_range(inventory_page_size):
        index = page * inventory_page_size + slot
        x = 88 + slot % 4 * 146
        y = 202 + math_floor(slot / 4) * 106
        color = vec3(0.08, 0.135, 0.145)
        if index == selected_item and index < size:
            color = vec3(0.18, 0.28, 0.27)
        else:
            if index < size and inside(x, y, 134, 94):
                color = vec3(0.12, 0.21, 0.21)
        shape("rect", x, y, 134, 94, color)
        if index < size:
            row = list_at(rows, index)
            info = item_presentation(row)
            if index == selected_item:
                shape("rect", x, y, 134, 2, gold_color)
                shape("rect", x, y, 2, 94, gold_color)
            if row.equipment:
                shape("rect", x, y + 91, 134, 3, rarity_color(info.rarity))
                if info.available:
                    weapon_emblem(x + 68, y + 12, 0.78, info)
                else:
                    label("?", x + 40, y + 14, 54, 34, muted, "center")
                label("Nv. " + number_text(info.level), x + 8, y + 4, 62, 12, muted)
                if info.equipped:
                    shape("diamond", x + 113, y + 10, 12, 12, accent)
                label(info.name, x + 8, y + 65, 118, 12, ink, "center")
            else:
                shape("diamond", x + 48, y + 18, 38, 38, muted)
                label("×" + number_text(row.stack.count), x + 76, y + 6, 50, 13, gold_color, "right")
                label(info.name, x + 8, y + 65, 118, 12, ink, "center")
    if size == 0:
        label("Nenhum item nesta categoria.", 100, 302, 550, 20, muted, "center")
    button("← Anterior", 88, 532, 158)
    label("Página " + number_text(page + 1) + " / " + number_text(maximum(1, math_floor((size + inventory_page_size - 1) / inventory_page_size))), 258, 544, 210, 16, muted, "center")
    button("Próxima →", 500, 532, 158)
    shape("rect", 88, 594, 570, 52, vec3(0.10, 0.17, 0.18))
    equipped = equipped_item()
    if equipped != null:
        current = weapon_definition()
        label("EM MÃOS", 104, 599, 96, 11, accent)
        label(current.name, 104, 618, 375, 18)
        label(number_text(current.damage) + " dano", 490, 610, 152, 18, gold_color, "right")
    else:
        label("Sem arma equipada", 104, 605, 500, 18, muted)
    shape("rect", 701, 144, 1, 502, vec3(0.22, 0.31, 0.30))
    if size > 0:
        row = list_at(rows, selected_item)
        info = item_presentation(row)
        label(info.name, 738, 148, 452, 28)
        if row.equipment:
            label("NÍVEL " + number_text(info.level) + "  ·  " + info.rarity, 740, 190, 410, 16, rarity_color(info.rarity))
            label("Cópia #" + number_text(row.item.uid), 740, 219, 410, 12, muted)
            if info.available:
                weapon_emblem(814, 264, 1.5, info)
                label(number_text(info.damage), 923, 274, 226, 52, ink, "right")
                label("DANO POR ACERTO", 923, 339, 226, 12, muted, "right")
                baseline = info
                if equipped != null:
                    baseline = weapon_definition()
                draw_stat("Dano", info.damage, baseline.damage, 394)
                draw_stat("Alcance", info.reach, baseline.reach, 434)
                draw_stat("Intervalo de ataque", info.windup + info.recovery, baseline.windup + baseline.recovery, 474, false, " s")
                pattern = "Corte em arco"
                if info.pattern == "thrust":
                    pattern = "Estocada frontal"
                label(pattern, 750, 518, 398, 16, muted)
                text = "E  Equipar arma"
                if info.equipped:
                    text = "Arma equipada"
                button(text, 740, 557, 434, not info.equipped)
                label("Variações comparadas à arma em mãos.", 750, 611, 410, 13, muted)
            else:
                label("MOD INDISPONÍVEL", 750, 300, 410, 22, gold_color)
                label("Esta cópia e seus atributos estão preservados.", 750, 342, 410, 15, muted)
                label("Reative o mod para voltar a usar o item.", 750, 372, 410, 15, muted)
        else:
            label("ITEM DO INVENTÁRIO", 740, 190, 410, 16, muted)
            shape("diamond", 912, 270, 70, 70, muted)
            label("Quantidade: " + number_text(row.stack.count), 750, 385, 410, 24, gold_color)
            label("Este item não ocupa o espaço de arma.", 750, 431, 410, 16, muted)
    else:
        label("Selecione um item", 738, 280, 436, 26, muted, "center")
    label("WASD / Q  Selecionar    E  Equipar    Clique  Comparar", 88, 650, 850, 12, muted)
    if notice_time > 0:
        label(notice, 738, 645, 452, 12, gold_color)

func equip_selected():
    rows = inventory_rows()
    if list_count(rows) == 0:
        return
    row = list_at(rows, selected_item % list_count(rows))
    if row.equipment:
        if equip_instance(row.item):
            notice = "Arma equipada: " + weapon_definition().name
        else:
            if not definition_exists("Items", row.item.definition):
                notice = "Reative o mod deste item para equipá-lo."
    else:
        notice = "Este item não é uma arma equipável."

func inventory_page(direction):
    size = list_count(inventory_rows())
    pages = maximum(1, math_floor((size + inventory_page_size - 1) / inventory_page_size))
    page = math_floor(selected_item / inventory_page_size)
    selected_item = ((page + pages + direction) % pages) * inventory_page_size

func inventory_navigation():
    size = list_count(inventory_rows())
    if size == 0:
        return
    if action_pressed("MoveLeft"):
        selected_item = (selected_item + size - 1) % size
    if action_pressed("MoveRight"):
        selected_item = (selected_item + 1) % size
    if action_pressed("MoveForward"):
        selected_item = maximum(0, selected_item - 4)
    if action_pressed("MoveBackward"):
        selected_item = minimum(size - 1, selected_item + 4)

func inventory_click():
    for index in number_range(3):
        if inside(88 + index * 194, 144, 182, 38):
            inventory_filter = index
            selected_item = 0
            return
    rows = inventory_rows()
    page = math_floor(selected_item / inventory_page_size)
    for slot in number_range(inventory_page_size):
        if inside(88 + slot % 4 * 146, 202 + math_floor(slot / 4) * 106, 134, 94):
            index = page * inventory_page_size + slot
            if index < list_count(rows):
                selected_item = index
            return
    if inside(88, 532, 158, 48):
        inventory_page(-1)
    if inside(500, 532, 158, 48):
        inventory_page(1)
    if inside(740, 557, 434, 48):
        equip_selected()
