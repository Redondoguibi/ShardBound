func begin_attack():
    if state.attack > 0 or health(hero) <= 0 or equipped_item() == null:
        return false
    spec = weapon_definition()
    validate_weapon(spec)
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
    hit.weapon = state.swing
    hit.item_uid = state.equipped_uid
    hit = apply_hook("adventure:melee", hit)
    applied = damage_from(hero, hit.target, hit.amount)
    if applied > 0:
        impact_position = vec3(hit.target.position.x, hit.target.position.y, hit.target.position.z)
        impact_time = 0.18
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
    for foe in get_entities_with(Sentinel):
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
    spec = get_definition("Entities", entity_definition(foe))
    brain.timer = spec.respawn
    look = get_entity_by_key(brain.visual)
    look.active = false
    where = vec3(foe.position.x, 0, foe.position.z)
    teleport(foe, vec3(brain.home.x, -20, brain.home.z))
    set_velocity(foe, vec3(0, 0, 0))
    ? Commit defeat before extension content can fail while creating a drop.
    set_component(foe, Sentinel, brain)
    state.gold = state.gold + spec.reward
    for item in spec.drops:
        drop_item(item, where)
    notice = "Inimigo derrotado. Ouro recebido; loot no chao."

func enemy_tick(foe, delta):
    brain = get_component(foe, Sentinel)
    spec = get_definition("Entities", entity_definition(foe))
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
            can_chase = not in_safe_region() and gap < spec.aggro and health(hero) > 0
            speed = 0
            heading = direction(foe.position, brain.home)
            if can_chase:
                heading = direction(foe.position, hero.position)
                if gap > spec.stop and not brain.winding:
                    speed = spec.speed
                if gap <= spec.trigger and brain.timer == 0 and not brain.winding:
                    brain.winding = true
                    brain.timer = spec.windup
            else:
                brain.winding = false
                if distance_xz(foe.position, brain.home) > 0.25:
                    speed = spec.speed
            if brain.winding and brain.timer == 0:
                if can_chase and gap < spec.reach and line_of_sight(foe, hero):
                    damage_from(foe, hero, spec.damage)
                    play_sound("core:hit")
                brain.winding = false
                brain.timer = spec.recovery
            if brain.winding:
                set_tint(look, vec3(1, 0.75, 0.25))
            else:
                set_tint(look, spec.tint)
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
    for foe in get_entities_with(Sentinel):
        enemy_tick(foe, delta)
    collect_loot()
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
    if speed > 0.1 and state.attack == 0:
        facing = vec3(motion.x / speed, 0, motion.z / speed)
    set_rotation(avatar, vec3(0, math_atan2(-facing.x, -facing.z), 0))
    animate_rig(avatar, speed, state.attack > 0)
    if not is_grounded(hero):
        pose_part(avatar, "LeftLeg", vec3(-0.125, 0.75, 0), vec3(0.3, 0, 0))
        pose_part(avatar, "RightLeg", vec3(0.125, 0.75, 0), vec3(-0.3, 0, 0))
        pose_part(avatar, "LeftArm", vec3(-0.25, 1.375, 0), vec3(-0.4, 0, -0.2))
    arm = attack_arm()
    if state.attack == 0:
        arm = math_sin(walk_time * 10) * 0.55
        if speed < 0.1:
            arm = 0
    pose_part(avatar, "RightArm", vec3(0.25, 1.375, 0), vec3(arm, 0, 0))
    ? Grip and scale belong to the equipped content definition.
    angle = arm + 3.141592654
    visual = weapon_definition()
    grip = visual.grip * visual.visual_scale.y
    hand_y = 1.375 - 0.625 * math_sin(arm + 1.570796327)
    hand_z = -0.625 * math_sin(arm)
    blade.position = vec3(0.375, hand_y - grip * math_sin(angle + 1.570796327), hand_z - grip * math_sin(angle))
    set_rotation(blade, vec3(angle, 0, 0))
