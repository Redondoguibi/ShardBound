func explore_tick(delta):
    state.combat = countdown(state.combat, delta)
    for entry in destination_list():
        flag = discovery(entry.id)
        stone = get_entity(entry.data.label)
        if stone != null:
            if distance_xz(hero.position, stone.position) <= entry.data.radius:
                if not flag.active:
                    notice = "Waystone descoberta: " + entry.data.name
                    emit_event("adventure:discovered", [entry.id])
                flag.active = true
            if flag.active:
                set_tint(stone, vec3(0.65, 1, 0.9))
    state.hub = discovered("adventure:Hub")
    state.forest = discovered("adventure:Forest")
    state.grove = discovered("adventure:Grove")
    if in_safe_region():
        heal(hero, delta * 8)
    if hero.position.y < -8:
        damage(hero, 100000)

func travel_destination(id):
    if not definition_exists("Waystones", id) or not discovered(id):
        notice = "Encontre esta waystone primeiro."
        return false
    if state.combat > 0:
        notice = "Viagem bloqueada: aguarde 6s sem dar ou receber dano."
        return false
    spec = get_definition("Waystones", id)
    if get_entity(spec.label) == null:
        notice = "Destino indisponivel neste mundo salvo."
        return false
    teleport(hero, spec.destination)
    state.attack = 0
    state.winding = false
    map_open = false
    notice = "Viagem concluida: " + spec.name
    emit_event("adventure:travelled", [spec.label])
    return true

func travel(label, legacy_flag):
    ? Legacy entry point never trusts the caller's discovery flag.
    for entry in destination_list():
        if entry.data.label == label:
            return travel_destination(entry.id)
    return false

func respawn_if_dead():
    if health(hero) > 0:
        return
    lost = math_floor(state.gold * 0.15)
    state.gold = state.gold - lost
    teleport(hero, get_definition("Worlds", "adventure:Main").respawn)
    heal(hero, maximum_health(hero))
    state.attack = 0
    state.winding = false
    notice = "Voce voltou ao abrigo. Ouro perdido: " + number_text(lost)
    emit_event("adventure:respawned", [lost])

@Events
on_damage(victim, amount):
    if hero != null and amount > 0:
        if entity_key(victim) == entity_key(hero):
            state.combat = 6

on_damage_dealt(source, victim, amount):
    if hero != null and amount > 0:
        if entity_key(source) == entity_key(hero):
            state.combat = 6
