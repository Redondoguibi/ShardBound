func map_marker(x, z, label, active):
    marker = record()
    marker.x = x
    marker.z = z
    marker.label = label
    marker.active = active
    return marker

func inventory_rows():
    rows = []
    for item in state.items:
        if item.owned:
            row = record()
            row.equipment = true
            row.item = item
            list_append(rows, row)
    for stack in inventory_entries(hero):
        tracked = false
        for item in state.items:
            if item.owned and item.definition == stack.id:
                tracked = true
        if not tracked:
            row = record()
            row.equipment = false
            row.stack = stack
            list_append(rows, row)
    return rows

func refresh_hud():
    region = current_region()
    region_name = "EXPLORACAO"
    if region != null:
        region_name = region.name
    spec = weapon_definition()
    text = "SHARDBOUND | " + region_name + " | Vida " + number_text(health(hero)) + " | Ouro " + number_text(state.gold)
    text = text + "\n" + spec.name + " Nv." + number_text(spec.level) + " " + spec.rarity
    if equipped_item() == null:
        text = text + " (sem arma equipada)"
    text = text + "\nWASD mover  Espaco saltar  Mouse atacar  M mapa  I inventario"
    text = text + "\nF5 salvar  F9 carregar  Q selecionar/trocar  E confirmar"
    destinations = destination_list()
    markers = [map_marker(hero.position.x, hero.position.z, "Voce", true)]
    for index in number_range(list_count(destinations)):
        entry = list_at(destinations, index)
        label = entry.data.name
        if index == selected_destination:
            label = "> " + label
        list_append(markers, map_marker(entry.data.position.x, entry.data.position.z, label, discovered(entry.id)))
    if inventory_open:
        rows = inventory_rows()
        size = list_count(rows)
        if selected_item >= size:
            selected_item = 0
        text = text + "\nINVENTARIO | Q proximo | E equipar | I fechar"
        page = math_floor(selected_item / 5)
        text = text + "\nPagina " + number_text(page + 1) + " | Entradas " + number_text(size)
        for index in number_range(size):
            if math_floor(index / 5) == page:
                row = list_at(rows, index)
                prefix = "  "
                if index == selected_item:
                    prefix = "> "
                if row.equipment:
                    item = row.item
                    equipped = ""
                    if item.uid == state.equipped_uid:
                        equipped = " [equipada]"
                    if definition_exists("Items", item.definition):
                        details = data_merge(get_definition("Items", item.definition), item.attributes)
                        text = text + "\n" + prefix + details.name + " #" + number_text(item.uid) + equipped
                        text = text + "\n  Nv." + number_text(item.level) + " " + item.rarity + " | Dano " + number_text(details.damage) + " | Alcance " + number_text(details.reach * 10) + "/10"
                    else:
                        text = text + "\n" + prefix + item.definition + " #" + number_text(item.uid) + "\n  Mod ausente; atributos preservados."
                else:
                    text = text + "\n" + prefix + row.stack.id + " x" + number_text(row.stack.count) + "\n  Item do inventario."
        text = text + "\n" + notice
    else:
        if map_open:
            if list_count(destinations) > 0:
                selected_destination = selected_destination % list_count(destinations)
                target = list_at(destinations, selected_destination)
                text = text + "\nQ proximo | E viajar: " + target.data.name + " | 1/2/3 atalhos"
            text = text + "\nBloqueio de combate: " + number_text(math_floor(state.combat + 0.999)) + "s"
        else:
            text = text + "\n" + notice + "\nExplore livremente. O abrigo recupera sua vida."
    show_ui(text, map_open, markers, inventory_open)

func handle_input():
    if action_pressed("Map"):
        map_open = not map_open
        inventory_open = false
    if action_pressed("Inventory"):
        inventory_open = not inventory_open
        map_open = false
        selected_item = 0
    if action_pressed("Jump") and not map_open and not inventory_open:
        jump(get_definition("Entities", Actor).jump)
    if action_pressed("Attack") and not map_open and not inventory_open:
        begin_attack()
    if action_pressed("Equip"):
        if inventory_open:
            size = list_count(inventory_rows())
            if size > 0:
                selected_item = (selected_item + 1) % size
        else:
            if map_open:
                size = list_count(destination_list())
                if size > 0:
                    selected_destination = (selected_destination + 1) % size
            else:
                if state.attack == 0:
                    cycle_equipment()
    if action_pressed("Interact"):
        if inventory_open:
            rows = inventory_rows()
            if list_count(rows) > 0:
                row = list_at(rows, selected_item % list_count(rows))
                if row.equipment:
                    equip_instance(row.item)
                else:
                    notice = "Este item nao e uma arma equipavel."
        if map_open:
            destinations = destination_list()
            if list_count(destinations) > 0:
                travel_destination(list_at(destinations, selected_destination % list_count(destinations)).id)
    if map_open:
        destinations = destination_list()
        for index in number_range(3):
            action = "Travel" + number_text(index + 1)
            if action_pressed(action) and index < list_count(destinations):
                travel_destination(list_at(destinations, index).id)
    if action_pressed("Save"):
        save_game("adventure")
    if action_pressed("Load"):
        load_game("adventure")
