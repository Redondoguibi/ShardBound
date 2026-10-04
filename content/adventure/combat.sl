func begin_attack():
    if state.attack > 0 or health(hero) <= 0:
        return false
    spec = weapon_definition()
    state.attack = spec.windup
    state.winding = true
    state.swing = spec
    state.aim = vec3(facing.x, 0, facing.z)
    return true

func strike_target(target, amount):
    hit = record()
    hit.source = hero
    hit.target = target
    hit.amount = amount
    hit = apply_hook("adventure:melee", hit)
    applied = damage_from(hero, hit.target, hit.amount)
    if applied > 0:
        play_sound("core:hit")
        if has_component(hit.target, Sentinel):
            hit_brain = get_component(hit.target, Sentinel)
            hit_brain.flash = 0.18
            set_component(hit.target, Sentinel, hit_brain)
    return applied

func resolve_attack():
    spec = state.swing
    amount = spec.damage
    if get_velocity(hero).y < 0 and not is_grounded(hero):
        amount = amount * 1.7
        notice = "Critico de queda: 1,7x!"
    for foe in get_entities("adventure:Enemy"):
        if health(foe) > 0:
            toward = direction(hero.position, foe.position)
            dot = toward.x * state.aim.x + toward.z * state.aim.z
            dy = hero.position.y - foe.position.y
            if distance_xz(hero.position, foe.position) <= spec.reach and dot >= spec.arc and dy < 2.5 and dy > -2.5:
                if line_of_sight(hero, foe):
                    strike_target(foe, amount)
    emit_event("adventure:attacked", [state.equipped])

func defeat_enemy(foe, brain):
    brain.dead = true
    brain.winding = false
    brain.timer = 25
    look = get_entity_by_key(brain.visual)
    look.active = false
    teleport(foe, vec3(brain.home.x, -20, brain.home.z))
    state.gold = state.gold + get_definition("Entities", Enemy).reward
    if item_count(hero, ForestSword) == 0:
        give_item(hero, ForestSword, 1)
        notice = "Espada incomum encontrada! I: inventario / Q: equipar."
    else:
        notice = "Inimigo derrotado. Ouro coletado."

func enemy_tick(foe, delta):
    brain = get_component(foe, Sentinel)
    spec = get_definition("Entities", Enemy)
    look = get_entity_by_key(brain.visual)
    brain.timer = countdown(brain.timer, delta)
    brain.flash = countdown(brain.flash, delta)
    if brain.dead:
        set_velocity(foe, vec3(0, 0, 0))
        if brain.timer == 0:
            heal(foe, maximum_health(foe))
            teleport(foe, brain.home)
            brain.dead = false
            look.active = true
    else:
        if health(foe) == 0:
            defeat_enemy(foe, brain)
        else:
            gap = distance_xz(foe.position, hero.position)
            can_chase = hero.position.z >= 7 and gap < 8 and health(hero) > 0
            speed = 0
            heading = direction(foe.position, brain.home)
            if can_chase:
                heading = direction(foe.position, hero.position)
                if gap > 1.35 and not brain.winding:
                    speed = spec.speed
                if gap <= 1.65 and brain.timer == 0 and not brain.winding:
                    brain.winding = true
                    brain.timer = spec.windup
            else:
                brain.winding = false
                if distance_xz(foe.position, brain.home) > 0.25:
                    speed = spec.speed
            if brain.winding and brain.timer == 0:
                if can_chase and gap < 1.9 and line_of_sight(foe, hero):
                    damage_from(foe, hero, spec.damage)
                    play_sound("core:hit")
                brain.winding = false
                brain.timer = spec.recovery
            if brain.winding:
                set_tint(look, vec3(1, 0.75, 0.25))
            else:
                set_tint(look, vec3(0.9, 0.45, 0.45))
            if brain.flash > 0:
                set_tint(look, vec3(1, 1, 1))
            vertical = get_velocity(foe).y
            set_velocity(foe, vec3(heading.x * speed, vertical, heading.z * speed))
            set_rotation(look, vec3(0, math_atan2(-heading.x, -heading.z), 0))
            animate_rig(look, speed, brain.winding)
    set_component(foe, Sentinel, brain)

func combat_tick(delta):
    state.attack = countdown(state.attack, delta)
    if state.winding and state.attack == 0:
        if health(hero) > 0:
            resolve_attack()
        state.winding = false
        state.attack = state.swing.recovery
    for foe in get_entities("adventure:Enemy"):
        enemy_tick(foe, delta)
    respawn_if_dead()

func animate_rig(look, speed, attacking):
    stride = math_sin(walk_time * 10) * 0.55
    if speed < 0.1:
        stride = 0
    pose_part(look, "LeftLeg", vec3(-0.125, 0.75, 0), vec3(stride, 0, 0))
    pose_part(look, "RightLeg", vec3(0.125, 0.75, 0), vec3(-stride, 0, 0))
    pose_part(look, "LeftArm", vec3(-0.25, 1.375, 0), vec3(-stride, 0, 0))
    arm = stride
    if attacking:
        arm = -1.6
    pose_part(look, "RightArm", vec3(0.25, 1.375, 0), vec3(arm, 0, 0))

func animate_hero(delta):
    walk_time = walk_time + delta
    motion = get_velocity(hero)
    speed = math_sqrt(motion.x * motion.x + motion.z * motion.z)
    if speed > 0.1 and not state.winding:
        facing = vec3(motion.x / speed, 0, motion.z / speed)
    set_rotation(avatar, vec3(0, math_atan2(-facing.x, -facing.z), 0))
    animate_rig(avatar, speed, state.attack > 0)
    if not is_grounded(hero):
        pose_part(avatar, "LeftLeg", vec3(-0.125, 0.75, 0), vec3(0.3, 0, 0))
        pose_part(avatar, "RightLeg", vec3(0.125, 0.75, 0), vec3(-0.3, 0, 0))
        pose_part(avatar, "LeftArm", vec3(-0.25, 1.375, 0), vec3(-0.4, 0, -0.2))
    if state.attack > 0:
        if state.swing.pattern == "thrust":
            set_rotation(blade, vec3(-1.2, 0, 0))
        else:
            set_rotation(blade, vec3(-0.7, 0, math_sin(state.attack * 9) * 0.8))
    else:
        set_rotation(blade, vec3(0, 0, 0))
