func discover_stone(label, flag):
    stone = get_entity(label)
    if distance_xz(hero.position, stone.position) <= 2.5:
        if not flag:
            notice = "Waystone descoberta! Abra o mapa com M."
        set_tint(stone, vec3(0.65, 1, 0.9))
        return true
    return flag

func explore_tick(delta):
    state.combat = countdown(state.combat, delta)
    state.hub = discover_stone("hub_waystone", state.hub)
    state.forest = discover_stone("forest_waystone", state.forest)
    state.grove = discover_stone("grove_waystone", state.grove)
    if hero.position.z < 7:
        heal(hero, delta * 8)
    if hero.position.y < -8:
        damage(hero, 100000)

func travel(label, discovered):
    if not discovered:
        notice = "Encontre esta waystone primeiro."
        return false
    if state.combat > 0:
        notice = "Viagem bloqueada: aguarde 6s sem dar ou receber dano."
        return false
    where = get_entity(label).position
    teleport(hero, vec3(where.x, 1.1, where.z - 2))
    state.attack = 0
    state.winding = false
    map_open = false
    notice = "Viagem concluida."
    emit_event("adventure:travelled", [label])
    return true

func respawn_if_dead():
    if health(hero) > 0:
        return
    lost = math_floor(state.gold * 0.15)
    state.gold = state.gold - lost
    teleport(hero, vec3(0, 1.1, 0))
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
