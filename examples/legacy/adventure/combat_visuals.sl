@Globals
impact_time = 0
impact_position = vec3(0, 0, 0)

func visual_mark(label, color):
    mark = get_entity(label)
    if mark == null:
        mark = block_at(vec3(0, -20, 0), vec3(0.06, 0.025, 0.06), color, false)
        mark.name = label
    mark.active = false
    return mark

func build_combat_visuals():
    reach_marks = []
    impact_marks = []
    for index in number_range(13):
        list_append(reach_marks, visual_mark("attack_reach_" + number_text(index), vec3(0.9, 0.8, 0.4)))
    for index in number_range(4):
        list_append(impact_marks, visual_mark("attack_impact_" + number_text(index), vec3(1, 0.9, 0.6)))
    impact_time = 0

func animate_combat_visuals(delta):
    spec = state.swing
    half_arc = math_atan2(math_sqrt(maximum(0, 1 - spec.arc * spec.arc)), spec.arc)
    heading = math_atan2(state.aim.x, state.aim.z)
    for index in number_range(list_count(reach_marks)):
        mark = list_at(reach_marks, index)
        mark.active = state.winding
        angle = heading - half_arc + 2 * half_arc * index / 12
        mark.position = vec3(hero.position.x + math_sin(angle) * spec.reach, 0.045, hero.position.z + math_sin(angle + 1.570796327) * spec.reach)
    impact_time = countdown(impact_time, delta)
    for index in number_range(list_count(impact_marks)):
        mark = list_at(impact_marks, index)
        mark.active = impact_time > 0
        spread = (0.18 - impact_time) * 3
        angle = index * 1.570796327
        mark.position = vec3(impact_position.x + math_sin(angle) * spread, impact_position.y, impact_position.z + math_sin(angle + 1.570796327) * spread)
        set_scale(mark, vec3(0.035, 0.035 + impact_time * 0.5, 0.035))

func attack_arm():
    if state.attack == 0:
        return 0
    if state.winding:
        progress = 1 - state.attack / maximum(0.001, state.swing.windup)
        return -0.3 - progress * 0.9
    progress = 1 - state.attack / maximum(0.001, state.swing.recovery)
    if state.swing.pattern == "thrust":
        return -1.7 + progress * 1.7
    return -2.4 + progress * 2.4
