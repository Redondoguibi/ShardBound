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
    if inventory_filter == 0:
        return rows
    filtered = []
    for row in rows:
        if (inventory_filter == 1 and row.equipment) or (inventory_filter == 2 and not row.equipment):
            list_append(filtered, row)
    return filtered

func handle_input():
    clicked_panel = action_pressed("Attack") and (inventory_open or map_open or paused)
    if action_pressed("Quit"):
        if map_open or inventory_open:
            map_open = false
            inventory_open = false
        else:
            if not title_open:
                paused = not paused
                pause_game(paused)
                menu_selected = 0
        refresh_hud()
    if paused:
        attack_buffer = 0
        menu_input()
        return
    canvas_input()
    movement_enabled(not map_open and not inventory_open)
    if clicked_panel:
        refresh_hud()
        return
    if action_pressed("Map"):
        map_open = not map_open
        inventory_open = false
    if action_pressed("Inventory"):
        inventory_open = not inventory_open
        map_open = false
        selected_item = 0
        inventory_filter = 0
    if map_open or inventory_open:
        attack_buffer = 0
    if action_pressed("Jump") and not map_open and not inventory_open:
        jump(get_definition("Entities", Actor).jump)
    if action_pressed("Attack") and not map_open and not inventory_open:
        update_aim()
        if not begin_attack():
            attack_buffer = 0.14
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
            equip_selected()
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
    if inventory_open:
        inventory_navigation()
    movement_enabled(not map_open and not inventory_open)
    if action_pressed("Save"):
        save_game("adventure")
    if action_pressed("Load"):
        load_game("adventure")
