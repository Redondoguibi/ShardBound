? Counts remain the generic inventory contract. Instance rules belong to this package.
func create_equipment(definition):
    spec = get_definition("Items", definition)
    item = record()
    item.uid = state.next_item_uid
    state.next_item_uid = state.next_item_uid + 1
    item.definition = definition
    item.level = spec.level
    item.rarity = spec.rarity
    item.attributes = record()
    item.owned = true
    item = apply_hook("adventure:create_item", item)
    ? Identity and ownership cannot be rewritten by an attribute hook.
    item.uid = state.next_item_uid - 1
    item.definition = definition
    item.owned = true
    validate_item(item)
    list_append(state.items, item)
    return list_at(state.items, list_count(state.items) - 1)

func weapon_visuals(spec):
    ? Additive fields preserve weapons written against the previous contract.
    defaults = get_definition("Items", IronSword)
    if not record_has(spec, "model"):
        spec.model = defaults.model
    if not record_has(spec, "visual_scale"):
        spec.visual_scale = defaults.visual_scale
    if not record_has(spec, "grip"):
        spec.grip = defaults.grip
    return spec

func validate_weapon(spec):
    spec = weapon_visuals(spec)
    if text_length(spec.name) == 0 or text_length(spec.name) > 64:
        script_error("Weapon names must contain 1 to 64 UTF-8 bytes")
    if spec.damage < 0 or spec.reach <= 0 or spec.reach > 100 or spec.arc < -1 or spec.arc > 1 or spec.windup <= 0 or spec.recovery <= 0:
        script_error("Invalid weapon combat attributes")
    if text_length(spec.model) == 0 or spec.visual_scale.x <= 0 or spec.visual_scale.y <= 0 or spec.visual_scale.z <= 0 or spec.grip < 0:
        script_error("Invalid weapon model, scale or grip")
    if spec.pattern != "slash" and spec.pattern != "thrust":
        script_error("Unsupported weapon attack pattern")
    if spec.tint.x < 0 or spec.tint.x > 1 or spec.tint.y < 0 or spec.tint.y > 1 or spec.tint.z < 0 or spec.tint.z > 1:
        script_error("Weapon tint must be in [0,1]")

func validate_item(item):
    if item.uid < 1 or item.uid != math_floor(item.uid) or item.uid > 9007199254740990:
        script_error("Invalid equipment identity")
    if item.level < 1 or item.level != math_floor(item.level):
        script_error("Equipment level must be a positive integer")
    if text_length(item.rarity) == 0 or text_length(item.rarity) > 64 or text_length(item.definition) == 0 or text_length(item.definition) > 128:
        script_error("Equipment rarity must not be empty")
    if item.owned != true and item.owned != false:
        script_error("Invalid equipment ownership")
    record_has(item.attributes, "damage")
    if definition_exists("Items", item.definition):
        validate_weapon(data_merge(get_definition("Items", item.definition), item.attributes))

func sync_equipment():
    remaining = []
    for item in state.items:
        if item.owned:
            list_append(remaining, item)
    state.items = remaining
    for stack in inventory_entries(hero):
        if definition_exists("Items", stack.id):
            spec = get_definition("Items", stack.id)
            if record_has(spec, "slot") and spec.slot == "weapon":
                owned = 0
                for item in state.items:
                    if item.definition == stack.id and item.owned:
                        owned = owned + 1
                        if owned > stack.count:
                            item.owned = false
                if stack.count > owned:
                    if stack.count - owned + list_count(state.items) > 256:
                        script_error("Equipment capacity is 256 instances")
                    for unused in number_range(stack.count - owned):
                        create_equipment(stack.id)
    for item in state.items:
        if item.owned and definition_exists("Items", item.definition):
            if item_count(hero, item.definition) == 0:
                item.owned = false

func usable_item(item):
    return item.owned and definition_exists("Items", item.definition)

func equipped_item():
    for item in state.items:
        if item.uid == state.equipped_uid and usable_item(item):
            return item
    return null

func equip_instance(item):
    if state.attack > 0 or not usable_item(item):
        notice = "Equipamento indisponivel ou ataque em andamento."
        return false
    spec = weapon_visuals(data_merge(get_definition("Items", item.definition), item.attributes))
    validate_weapon(spec)
    if is_studio_model(blade):
        animation_stop(blade)
        studio_detach(blade)
        set_parent(blade, avatar)
    set_model(blade, spec.model)
    set_scale(blade, spec.visual_scale)
    set_tint(blade, spec.tint)
    if is_studio_model(blade):
        studio_attach(blade, avatar, true)
    state.equipped_uid = item.uid
    state.equipped = item.definition
    emit_event("adventure:equipped", [item.uid, item.definition])
    return true

func ensure_equipment():
    sync_equipment()
    if equipped_item() == null and state.attack == 0:
        for item in state.items:
            if usable_item(item):
                equip_instance(item)
                return

func next_equipment():
    size = list_count(state.items)
    if size == 0:
        return
    for step in number_range(size):
        selected_item = (selected_item + 1) % size
        if list_at(state.items, selected_item).owned:
            return

func cycle_equipment():
    for index in number_range(list_count(state.items)):
        if list_at(state.items, index).uid == state.equipped_uid:
            selected_item = index
    for unused in number_range(list_count(state.items)):
        next_equipment()
        item = list_at(state.items, selected_item)
        if usable_item(item):
            equip_instance(item)
            return
