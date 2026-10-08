? Official scenery is additive, versioned and independent of collision/game rules.
func detail(where, half, color, model="core:solid"):
    piece = block_at(where, half, color, false)
    if model != "core:solid":
        set_model(piece, model)
    return piece

func timber_house(x, z):
    wood = vec3(0.12, 0.066, 0.035)
    roof = vec3(0.09, 0.19, 0.20)
    detail(vec3(x, 0.16, z), vec3(2.13, 0.16, 1.83), vec3(0.25, 0.29, 0.29))
    for dx in [-1.94, 0, 1.94]:
        detail(vec3(x + dx, 1.5, z - 1.74), vec3(0.095, 1.35, 0.09), wood)
        detail(vec3(x + dx, 1.5, z + 1.74), vec3(0.095, 1.35, 0.09), wood)
    for dy in [0.45, 2.55]:
        detail(vec3(x, dy, z - 1.75), vec3(2.1, 0.09, 0.1), wood)
        detail(vec3(x, dy, z + 1.75), vec3(2.1, 0.09, 0.1), wood)
        detail(vec3(x - 2.02, dy, z), vec3(0.1, 0.09, 1.8), wood)
        detail(vec3(x + 2.02, dy, z), vec3(0.1, 0.09, 1.8), wood)
    for step in number_range(6):
        height = 2.92 + step * 0.25
        width = 2.55 - step * 0.4
        detail(vec3(x, height, z), vec3(width, 0.15, 2.18), roof, "core:roof")
        detail(vec3(x, height + 0.11, z - 2.19), vec3(width, 0.035, 0.04), vec3(0.20, 0.30, 0.29))
    detail(vec3(x + 1.1, 3.8, z + 0.6), vec3(0.28, 0.8, 0.28), vec3(0.27, 0.22, 0.19))
    detail(vec3(x + 1.1, 4.55, z + 0.6), vec3(0.35, 0.1, 0.35), vec3(0.18, 0.16, 0.14))
    detail(vec3(x, 0.12, z - 2.35), vec3(1.25, 0.12, 0.5), vec3(0.25, 0.19, 0.12))
    for dx in [-0.55, 0.55]:
        detail(vec3(x + dx, 0.95, z - 1.82), vec3(0.065, 0.85, 0.07), wood)
    detail(vec3(x, 1.85, z - 1.82), vec3(0.62, 0.075, 0.07), wood)
    detail(vec3(x + 0.3, 0.9, z - 1.84), vec3(0.055, 0.055, 0.05), vec3(0.65, 0.44, 0.12))
    for dx in [-1.08, 1.08]:
        detail(vec3(x + dx, 1.5, z - 1.84), vec3(0.33, 0.37, 0.04), vec3(0.78, 0.49, 0.16))
        detail(vec3(x + dx, 1.5, z - 1.9), vec3(0.035, 0.4, 0.025), wood)
        detail(vec3(x + dx, 1.5, z - 1.9), vec3(0.38, 0.035, 0.025), wood)
        detail(vec3(x + dx, 1.08, z - 1.95), vec3(0.48, 0.06, 0.15), wood)

func fence(x, z, length):
    wood = vec3(0.25, 0.17, 0.09)
    for index in number_range(length + 1):
        detail(vec3(x + index * 1.4, 0.55, z), vec3(0.10, 0.6, 0.1), wood)
    for y in [0.4, 0.85]:
        detail(vec3(x + length * 0.7, y, z), vec3(length * 0.7, 0.065, 0.07), wood)

func lamp(x, z):
    detail(vec3(x, 1.35, z), vec3(0.09, 1.35, 0.09), vec3(0.16, 0.11, 0.07))
    detail(vec3(x, 2.65, z), vec3(0.26, 0.06, 0.26), vec3(0.10, 0.14, 0.14))
    detail(vec3(x, 2.4, z), vec3(0.18, 0.22, 0.18), vec3(1, 0.64, 0.16))
    detail(vec3(x, 2.14, z), vec3(0.25, 0.05, 0.25), vec3(0.10, 0.14, 0.14))

func tuft(x, z, seed):
    color = vec3(0.12 + seed * 0.035, 0.25 + seed * 0.025, 0.085)
    for index in number_range(3):
        grass = detail(vec3(x + index * 0.11, 0.16 + index * 0.035, z), vec3(0.055, 0.16 + index * 0.035, 0.05), color)
        set_rotation(grass, vec3(0, seed, (index - 1) * 0.25))

func build_scenery():
    if get_entity("adventure_scenery_v1") != null:
        return
    tree_index = 0
    for tree in get_entities("adventure:Tree"):
        if tree_index % 3 != 1:
            set_model(tree, "core:pine")
            pine_size = 0.8 + (tree_index % 4) * 0.09
            set_scale(tree, vec3(pine_size, pine_size, pine_size))
            set_rotation(tree, vec3(0, tree_index * 1.1, 0))
        else:
            set_tint(tree, vec3(0.65, 0.22, 0.5))
        tree_index = tree_index + 1
    for piece in get_entities("adventure:Block"):
        if piece.position.x == 0 and piece.position.z == 17 and piece.position.y > 0.015 and piece.position.y < 0.017:
            piece.active = false
    ? Existing saves retain their houses and collision; the new facade covers them.
    for where in get_definition("Regions", "adventure:Hub").shelters:
        timber_house(where.x, where.z)
    detail(vec3(0, -0.55, 15), vec3(65, 0.04, 75), vec3(0.16, 0.25, 0.11))
    for row in number_range(17):
        z = -12 + row * 3.1
        for col in number_range(11):
            x = -16 + col * 3.1
            value = (row * 13 + col * 7) % 9
            color = vec3(0.22 + value * 0.0015, 0.35 + value * 0.002, 0.12 + value * 0.001)
            if z < 7:
                color = vec3(0.33 + value * 0.002, 0.35 + value * 0.002, 0.20 + value * 0.001)
            detail(vec3(x, 0.006, z), vec3(1.56, 0.006, 1.56), color, "core:grass")
            if (col < 4 or col > 6) and z > 7:
                tuft(x + value * 0.1, z + 0.3, value % 3)
    ? A broken flagstone walk leads to the trail; colored shoulders soften the straight collision-free route.
    for index in number_range(30):
        z = -4 + index * 1.4
        x = math_sin(index * 0.30) * 1.2
        detail(vec3(x, 0.028, z), vec3(1.35, 0.012, 0.75), vec3(0.46, 0.37, 0.22), "core:earth")
        if z < 8:
            for side in [-1, 1]:
                detail(vec3(x + side * 0.8, 0.048, z), vec3(0.65, 0.012, 0.52), vec3(0.42, 0.45, 0.36), "core:stone")
    for x in [-10, -7, -4, 4, 7, 10]:
        detail(vec3(x, 0.032, -6.2), vec3(1.48, 0.015, 0.7), vec3(0.39, 0.37, 0.28))
    fence(-12, -7.5, 5)
    fence(5, -7.5, 5)
    fence(-12, 6.2, 5)
    for where in [vec3(-3.8, 0, -6), vec3(3.8, 0, -6), vec3(-3.5, 0, 6), vec3(3.5, 0, 6)]:
        lamp(where.x, where.z)
    for where in [vec3(9.6, 0, -4.5), vec3(10.4, 0, -4.3), vec3(-9.7, 0, 3)]:
        detail(vec3(where.x, 0.4, where.z), vec3(0.38, 0.4, 0.38), vec3(0.35, 0.23, 0.10))
        for y in [0.15, 0.62]:
            detail(vec3(where.x, y, where.z), vec3(0.4, 0.035, 0.4), vec3(0.14, 0.16, 0.15))
    for entry in destination_list():
        p = entry.data.position
        for index in number_range(8):
            angle = index * 0.785398
            detail(vec3(p.x + math_sin(angle) * 1.25, 0.07, p.z + math_sin(angle + 1.5708) * 1.25), vec3(0.34, 0.07, 0.34), vec3(0.30, 0.37, 0.34))
    for side in [-1, 1]:
        for index in number_range(12):
            x = side * (14.5 + index % 3)
            z = -13 + index * 5.2
            tree = spawn(Pine, vec3(x, 0, z))
            size = 0.78 + (index % 4) * 0.10
            set_scale(tree, vec3(size, size, size))
            set_rotation(tree, vec3(0, index * 0.9, 0))
            rock = detail(vec3(side * 17.5, 0.25, z + 2), vec3(1.6, 0.7 + index % 3 * 0.3, 2.5), vec3(0.22, 0.28, 0.26))
            set_rotation(rock, vec3(0, index * 0.15, 0))
    for index in number_range(14):
        x = -14 + (index * 11 % 29)
        z = 9 + (index * 7 % 31)
        if x < -4 or x > 5:
            rock = detail(vec3(x, 0.18, z), vec3(0.5, 0.3, 0.65), vec3(0.27, 0.32, 0.28))
            set_rotation(rock, vec3(0, index, 0.15))
    marker = spawn(SceneryMarker, vec3(0, -50, 0))
    marker.name = "adventure_scenery_v1"
    marker.active = false

func configure_presentation():
    environment = record()
    environment.sky = vec3(0.36, 0.46, 0.46)
    environment.sun = vec3(0.94, 0.81, 0.59)
    environment.ambient = vec3(0.28, 0.35, 0.40)
    environment.fog_start = 27
    environment.fog_end = 65
    environment.shadows = true
    set_environment(environment)
    build_scenery()
