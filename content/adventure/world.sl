func block_at(where, scale, color, solid):
    piece = spawn(Block, where)
    set_scale(piece, scale)
    set_tint(piece, color)
    if solid:
        add_box(piece, scale, false)
    return piece

func shelter(x, z):
    block_at(vec3(x, 1.3, z), vec3(2, 1.3, 1.7), vec3(0.68, 0.49, 0.3), true)
    block_at(vec3(x, 2.9, z), vec3(2.4, 0.3, 2.1), vec3(0.26, 0.38, 0.42), false)
    block_at(vec3(x, 0.85, z - 1.72), vec3(0.5, 0.85, 0.03), vec3(0.18, 0.14, 0.12), false)
    block_at(vec3(x + 1.1, 1.5, z - 1.73), vec3(0.35, 0.35, 0.04), vec3(0.95, 0.8, 0.44), false)

func stone_at(label, where):
    stone = spawn(Waystone, where)
    stone.name = label
    set_scale(stone, vec3(0.055, 0.055, 0.055))
    set_tint(stone, vec3(0.5, 0.55, 0.6))
    return stone

func enemy_at(x, z):
    foe = spawn(Enemy, vec3(x, 1, z))
    add_box(foe, vec3(0.4, 1, 0.3), true)
    stats = get_definition("Entities", Enemy)
    set_health(foe, stats.max_health)
    look = spawn(Explorer, vec3(0, -1, 0))
    set_parent(look, foe)
    set_tint(look, vec3(0.9, 0.45, 0.45))
    add_component(foe, Sentinel)
    brain = get_component(foe, Sentinel)
    brain.visual = entity_key(look)
    brain.home = vec3(x, 1, z)
    set_component(foe, Sentinel, brain)

func build_world():
    block_at(vec3(0, -0.5, 14), vec3(18, 0.5, 28), vec3(0.3, 0.48, 0.25), true)
    block_at(vec3(0, 0.012, -3), vec3(12, 0.01, 9), vec3(0.62, 0.54, 0.37), false)
    block_at(vec3(0, 0.016, 17), vec3(1.8, 0.01, 21), vec3(0.53, 0.46, 0.32), false)
    for side in [-1, 1]:
        block_at(vec3(side * 18, 1.2, 14), vec3(0.4, 1.2, 28), vec3(0.27, 0.33, 0.29), true)
    for end in [-14, 42]:
        block_at(vec3(0, 1.2, end), vec3(18, 1.2, 0.4), vec3(0.27, 0.33, 0.29), true)
    shelter(-7, -3)
    shelter(7, -3)
    shelter(-7, 4)
    for z in [10, 17, 24, 32, 39]:
        for x in [-13, -8, 8, 13]:
            tree = spawn(Tree, vec3(x, 0, z))
            set_scale(tree, vec3(0.038, 0.038, 0.038))
            trunk = block_at(vec3(x, 1, z), vec3(0.3, 1, 0.3), vec3(0.35, 0.27, 0.19), true)
            trunk.active = false
    for x in [-4, 4]:
        block_at(vec3(x, 0.25, 12), vec3(0.8, 0.25, 0.8), vec3(0.55, 0.6, 0.59), true)
    stone_at("hub_waystone", vec3(3, 0.06, 0))
    stone_at("forest_waystone", vec3(-3, 0.06, 20))
    stone_at("grove_waystone", vec3(3, 0.06, 36))
    enemy_at(2, 14)
    enemy_at(-3, 26)
    enemy_at(4, 31)
    hero = spawn(Actor, vec3(0, 1.1, 0))
    hero.name = "player"
    add_box(hero, vec3(0.4, 1, 0.3), true)
    stats = get_definition("Entities", Actor)
    set_health(hero, stats.max_health)
    set_player(hero, stats.speed)
    avatar = spawn(Explorer, vec3(0, -1, 0))
    avatar.name = "player_visual"
    set_parent(avatar, hero)
    blade = spawn(Blade, vec3(0.38, -0.6, -0.2))
    blade.name = "player_sword"
    set_parent(blade, avatar)
    set_scale(blade, vec3(0.075, 0.075, 0.075))
    give_item(hero, IronSword, 1)
