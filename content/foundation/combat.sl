func gap(a, b):
    x = b.x - a.x
    z = b.z - a.z
    return math_sqrt(x * x + z * z)

func heading(a, b):
    length = gap(a, b)
    if length < 0.001:
        return vec3(0, 0, 1)
    return vec3((b.x - a.x) / length, 0, (b.z - a.z) / length)

func begin_attack():
    weapon = get_definition("Items", state.weapon)
    aerial = not is_grounded(hero)
    sequence = []
    if aerial and record_has(weapon, "air_attacks"):
        sequence = weapon.air_attacks
    if not aerial and record_has(weapon, "ground_attacks"):
        sequence = weapon.ground_attacks
    attack_profile = record()
    attack_profile.damage = weapon.damage
    attack_profile.reach = weapon.reach
    attack_profile.windup = weapon.windup
    attack_profile.recovery = weapon.recovery
    attack_profile.shape = "arc"
    attack_profile.width = weapon.reach * 2
    attack_profile.limit_arc_width = false
    attack_profile.height = null
    attack_profile.clip = "attack"
    if list_count(sequence) > 0:
        if combo_time <= 0 or aerial != combo_aerial:
            combo_index = 0
        step = get_definition("Attacks", list_at(sequence, combo_index % list_count(sequence)))
        combo_index = combo_index + 1
        combo_aerial = aerial
        if record_has(step, "damage"):
            attack_profile.damage = step.damage
        if record_has(step, "reach"):
            attack_profile.reach = step.reach
        if record_has(step, "windup"):
            attack_profile.windup = step.windup
        if record_has(step, "recovery"):
            attack_profile.recovery = step.recovery
        if record_has(step, "shape"):
            attack_profile.shape = step.shape
        if record_has(step, "width"):
            attack_profile.width = step.width
            attack_profile.limit_arc_width = true
        if record_has(step, "height"):
            attack_profile.height = step.height
        if record_has(step, "clip"):
            attack_profile.clip = step.clip
    attack_time = attack_profile.windup
    combo_time = attack_profile.windup + attack_profile.recovery + 0.65
    winding = true
    play_sound("foundation:swing")
    if is_studio_model(blade) and animation_has(blade, attack_profile.clip):
        animation_play(blade, attack_profile.clip, "once")

func resolve_hit():
    amount = attack_profile.damage
    if get_velocity(hero).y < 0 and not is_grounded(hero):
        amount = amount * get_definition("Rules", Gameplay).falling_critical
    for foe in get_entities_with(Brain):
        direction = heading(hero.position, foe.position)
        forward = direction.x * facing.x + direction.z * facing.z
        distance = gap(hero.position, foe.position)
        side = distance * (direction.x * facing.z - direction.z * facing.x)
        touches = distance <= attack_profile.reach and forward > -0.1
        if attack_profile.shape == "arc" and attack_profile.limit_arc_width:
            touches = touches and side >= -attack_profile.width / 2 and side <= attack_profile.width / 2
        if attack_profile.shape == "box":
            touches = forward >= 0 and distance * forward <= attack_profile.reach and side >= -attack_profile.width / 2 and side <= attack_profile.width / 2
        if attack_profile.shape == "circle":
            touches = distance <= attack_profile.reach
        if attack_profile.height != null:
            offset_y = foe.position.y - hero.position.y
            touches = touches and offset_y >= -attack_profile.height / 2 and offset_y <= attack_profile.height / 2
        if health(foe) > 0 and touches:
            if line_of_sight(hero, foe):
                damage_from(hero, foe, amount)
                play_sound("foundation:hit")

func combat_tick(delta):
    combo_time = combo_time - delta
    if attack_time > 0:
        attack_time = attack_time - delta
        if attack_time <= 0 and winding:
            winding = false
            resolve_hit()
            attack_time = attack_profile.recovery
    for foe in get_entities_with(Brain):
        brain = get_component(foe, Brain)
        stats = get_definition("Entities", entity_definition(foe))
        look = get_entity_by_key(brain.visual)
        if health(foe) <= 0:
            list_append(state.defeated, brain.key)
            state.gold = state.gold + stats.gold
            drop = spawn(Drop, vec3(foe.position.x, terrain_height(foe.position.x, foe.position.z) + 0.2, foe.position.z))
            set_scale(drop, vec3(0.035, 0.035, 0.035))
            add_component(drop, GroundDrop)
            loot = get_component(drop, GroundDrop)
            loot.item = stats.drop
            set_component(drop, GroundDrop, loot)
            destroy(look)
            destroy(foe)
        else:
            brain.timer = brain.timer - delta
            distance = gap(foe.position, hero.position)
            if foe.position.y < terrain_height(foe.position.x, foe.position.z) - 2:
                teleport(foe, vec3(foe.position.x, terrain_height(foe.position.x, foe.position.z) + stats.body.y + 0.1, foe.position.z))
            direction = heading(foe.position, hero.position)
            velocity = get_velocity(foe)
            speed = 0
            if distance < stats.aggro and distance > stats.reach and not brain.winding:
                speed = stats.speed
            if distance <= stats.reach and brain.timer <= 0 and not brain.winding:
                brain.winding = true
                brain.timer = stats.windup
                set_tint(look, vec3(1, 0.55, 0.4))
            if brain.winding and brain.timer <= 0:
                if distance <= stats.reach + 0.3 and line_of_sight(foe, hero):
                    damage_from(foe, hero, stats.damage)
                    play_sound("foundation:hit")
                brain.winding = false
                brain.timer = stats.recovery
                set_tint(look, vec3(1, 1, 1))
            set_velocity(foe, vec3(direction.x * speed, velocity.y, direction.z * speed))
            set_rotation(look, vec3(0, math_atan2(-direction.x, -direction.z), 0))
            if speed > 0 and is_grounded(foe):
                if terrain_height(foe.position.x + direction.x, foe.position.z + direction.z) > terrain_height(foe.position.x, foe.position.z):
                    set_velocity(foe, vec3(direction.x * speed, 5, direction.z * speed))
            set_component(foe, Brain, brain)

func animate_player():
    velocity = get_velocity(hero)
    speed = math_sqrt(velocity.x * velocity.x + velocity.z * velocity.z)
    if is_studio_model(blade):
        clip = "idle"
        if speed > 0.1:
            clip = "run"
        if not is_grounded(hero):
            clip = "jump"
        if attack_time > 0 and attack_profile != null:
            clip = attack_profile.clip
        if animation_has(blade, clip):
            if animation_current(blade) != clip:
                animation_play(blade, clip)
            return
        animation_stop(blade)
    stride = 0
    if speed > 0.1:
        stride = math_sin(walk_time * 10) * 0.55
    arm = stride
    if attack_time > 0:
        arm = -1.3
        if not winding:
            arm = 0.7
    pose_part(avatar, "LeftLeg", vec3(-0.125, 0.75, 0), vec3(stride, 0, 0))
    pose_part(avatar, "RightLeg", vec3(0.125, 0.75, 0), vec3(-stride, 0, 0))
    pose_part(avatar, "LeftArm", vec3(-0.25, 1.375, 0), vec3(-stride, 0, 0))
    pose_part(avatar, "RightArm", vec3(0.25, 1.375, 0), vec3(arm, 0, 0))
    angle = arm + 3.141592654
    grip = 7 * get_definition("Items", state.weapon).scale.y
    hand_y = 1.375 - 0.625 * math_sin(arm + 1.570796327)
    hand_z = -0.625 * math_sin(arm)
    blade.position = vec3(0.375, hand_y - grip * math_sin(angle + 1.570796327), hand_z - grip * math_sin(angle))
    set_rotation(blade, vec3(angle, 0, 0))
