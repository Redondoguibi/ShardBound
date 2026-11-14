? Repair presentation without adding areas, encounters or item definitions.
import "scenery_v1_shapes.sl"
@Globals
scenery_root = null

func remove_previous_scenery():
    for definition in ["adventure:Block", "adventure:Pine"]:
        for piece in get_entities(definition):
            if piece.name == definition:
                visual = entity_visual(piece)
                if not visual.physical and list_contains(["core:solid", "core:grass", "core:earth", "core:plaster", "core:roof", "core:stone", "core:pine"], visual.model):
                    if record_has(legacy_scenery_shapes, visual.signature):
                        destroy(piece)
    old = get_entity("adventure_scenery_v1")
    if old != null:
        destroy(old)

func detail(where, half, color, model="core:solid"):
    piece = block_at(where, half, color, false)
    piece.name = "adventure_scenery_block"
    set_parent(piece, scenery_root)
    if model != "core:solid":
        set_model(piece, model)
    return piece

func timber_house(x, z):
    detail(vec3(x, 1.5, z), vec3(2, 1.5, 2), vec3(0.74, 0.63, 0.44), "core:plaster")
    detail(vec3(x - 0.5, 1, z - 2.015), vec3(0.5, 1, 0.015), vec3(0.30, 0.20, 0.11), "core:earth")
    detail(vec3(x + 0.75, 1.5, z - 2.02), vec3(0.5, 0.5, 0.02), vec3(0.96, 0.77, 0.38))
    for side in [-1, 1]:
        detail(vec3(x + side * 1.94, 1.5, z), vec3(0.125, 1.5, 2.01), vec3(0.32, 0.23, 0.13))
    for step in number_range(3):
        detail(vec3(x, 3.25 + step * 0.5, z), vec3(3 - step, 0.25, 3), vec3(0.26, 0.42, 0.42), "core:roof")

func fence(x, z, length):
    for index in number_range(length + 1):
        detail(vec3(x + index * 2, 0.75, z), vec3(0.125, 0.75, 0.125), vec3(0.40, 0.28, 0.14))
    for y in [0.5, 1]:
        detail(vec3(x + length, y, z), vec3(length, 0.125, 0.125), vec3(0.40, 0.28, 0.14))

func lamp(x, z):
    detail(vec3(x, 1.25, z), vec3(0.125, 1.25, 0.125), vec3(0.34, 0.24, 0.13))
    detail(vec3(x, 2.5, z), vec3(0.25, 0.25, 0.25), vec3(1, 0.73, 0.25))
    detail(vec3(x, 2.875, z), vec3(0.375, 0.125, 0.375), vec3(0.22, 0.31, 0.31))

func build_scenery():
    scenery_root = get_entity("adventure_scenery_v2")
    if scenery_root != null:
        return
    unfinished = get_entity("adventure_scenery_building")
    if unfinished != null:
        for piece in get_entities("adventure:Block"):
            if piece.name == "adventure_scenery_block":
                destroy(piece)
        for tree in get_entities("adventure:Tree"):
            if tree.name == "adventure_scenery_tree":
                destroy(tree)
        destroy(unfinished)
    remove_previous_scenery()
    scenery_root = spawn(SceneryMarker, vec3(0, 0, 0))
    scenery_root.name = "adventure_scenery_building"
    scenery_root.active = false
    original_blocks = get_entities("adventure:Block")
    destinations = destination_list()
    for entry in region_catalog:
        if entry.id == "adventure:Hub" or entry.id == "adventure:Forest":
            region = entry.data
            for tree in get_entities("adventure:Tree"):
                if list_contains(region.trees, vec3(tree.position.x, tree.position.y, tree.position.z)):
                    set_model(tree, "core:tree")
                    set_scale(tree, vec3(0.038, 0.038, 0.038))
                    set_rotation(tree, vec3(0, 0, 0))
                    set_tint(tree, vec3(0.7, 0.45, 0.7))
            for piece in original_blocks:
                visual = entity_visual(piece)
                if visual.physical and piece.position.y == -0.5 and piece.position.x == (region.min.x + region.max.x) / 2 and piece.position.z == (region.min.z + region.max.z) / 2:
                    piece.active = false
                for where in region.shelters:
                    if visual.physical and piece.position.x == where.x and piece.position.z == where.z and piece.position.y > 1.29 and piece.position.y < 1.51:
                        teleport(piece, vec3(where.x, 1.5, where.z))
                        add_box(piece, vec3(2, 1.5, 2), false)
                        piece.active = false
            for where in region.shelters:
                timber_house(where.x, where.z)
            columns = math_floor((region.max.x - region.min.x) / 2)
            rows = math_floor((region.max.z - region.min.z + 1) / 2)
            for row in number_range(rows):
                z_half = minimum(1, (region.max.z - region.min.z - row * 2) / 2)
                z = region.min.z + row * 2 + z_half
                path_x = math_floor(math_sin((z + 4) / 1.4 * 0.30) * 1.2)
                terrain_color = vec3(0.26, 0.44, 0.16)
                if region.safe:
                    terrain_color = vec3(0.36, 0.45, 0.22)
                row_destinations = []
                for destination in destinations:
                    if z >= destination.data.position.z - 1.5 and z <= destination.data.position.z + 1.5:
                        list_append(row_destinations, destination)
                for col in number_range(columns):
                    x = region.min.x + col * 2 + 1
                    color = terrain_color
                    model = "core:grass"
                    if z >= -4 and z < 38 and x >= path_x - 1.5 and x <= path_x + 1.5:
                        color = vec3(0.52, 0.38, 0.20)
                        model = "core:earth"
                    for destination in row_destinations:
                        p = destination.data.position
                        if x >= p.x - 1.5 and x <= p.x + 1.5 and z >= p.z - 1.5 and z <= p.z + 1.5:
                            color = vec3(0.57, 0.61, 0.57)
                            model = "core:stone"
                    detail(vec3(x, -1, z), vec3(1, 1, z_half), color, model)
    fence(-12, -8, 3)
    fence(6, -8, 3)
    fence(-12, 6, 3)
    for where in [vec3(-4, 0, -6), vec3(4, 0, -6), vec3(-4, 0, 6), vec3(4, 0, 6)]:
        lamp(where.x, where.z)
    for side in [-1, 1]:
        for index in number_range(12):
            tree = spawn(Tree, vec3(side * (15 + index % 3), 0, -13 + math_floor(index * 5.2)))
            tree.name = "adventure_scenery_tree"
            set_parent(tree, scenery_root)
            set_scale(tree, vec3(0.038, 0.038, 0.038))
            set_tint(tree, vec3(0.7, 0.45, 0.7))
    scenery_root.name = "adventure_scenery_v2"

func configure_presentation():
    environment = record()
    environment.sky = vec3(0.43, 0.54, 0.65)
    environment.sun = vec3(0.94, 0.90, 0.79)
    environment.ambient = vec3(0.32, 0.38, 0.45)
    environment.fog_start = 40
    environment.fog_end = 85
    environment.shadows = true
    set_environment(environment)
    build_scenery()
