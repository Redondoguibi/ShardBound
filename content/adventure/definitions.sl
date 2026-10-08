? Provisional tuning. Definitions are public and patchable by dependent mods.
@Entities
entity Actor:
    .max_health = 100
    .speed = 4
    .jump = 6
entity Enemy:
    .visual = "adventure:Explorer"
    .body = vec3(0.4, 1, 0.3)
    .visual_scale = vec3(1, 1, 1)
    .tint = vec3(0.9, 0.45, 0.45)
    .aggro = 8
    .stop = 1.35
    .trigger = 1.65
    .reach = 1.9
    .respawn = 25
    .drops = ["adventure:ForestSword"]
    .max_health = 45
    .speed = 1.8
    .damage = 12
    .windup = 0.65
    .recovery = 1.2
    .reward = 12
entity Explorer:
    Assets:
        .model = "core:player"
entity ScoutVisual:
    Assets:
        .model = "core:scout"
entity BruteVisual:
    Assets:
        .model = "core:brute"
entity Scout from Enemy:
    .visual = "adventure:ScoutVisual"
    .body = vec3(0.32, 0.9, 0.28)
    .visual_scale = vec3(0.9, 0.9, 0.9)
    .tint = vec3(0.9, 0.95, 0.9)
    .max_health = 35
    .speed = 2.7
    .damage = 8
    .windup = 0.4
    .recovery = 0.75
    .reward = 9
    .stop = 1.1
    .trigger = 1.35
    .reach = 1.6
    .drops = ["adventure:TrailKnife"]
entity Brute from Enemy:
    .visual = "adventure:BruteVisual"
    .body = vec3(0.55, 1.15, 0.4)
    .visual_scale = vec3(1.15, 1.15, 1.15)
    .tint = vec3(0.9, 0.9, 0.95)
    .max_health = 100
    .speed = 1.2
    .damage = 24
    .windup = 1.1
    .recovery = 1.6
    .reward = 24
    .stop = 1.6
    .trigger = 2
    .reach = 2.2
    .drops = ["adventure:HeavyBlade"]
entity SceneryMarker:
    .version = 1
entity Block:
    Assets:
        .model = "core:solid"
entity Tree:
    Assets:
        .model = "core:tree"
entity Pine:
    Assets:
        .model = "core:pine"
entity Waystone:
    Assets:
        .model = "core:waystone"
entity Blade:
    Assets:
        .model = "core:sword"
entity LootVisual:
    Assets:
        .model = "core:iron_sword"

@Items
weapon IronSword:
    .model = "core:iron_sword"
    .visual_scale = vec3(0.05, 0.05, 0.05)
    .grip = 7
    .slot = "weapon"
    .tint = vec3(1, 1, 1)
    .name = "Espada de ferro"
    .level = 1
    .rarity = "Comum"
    .damage = 15
    .reach = 2.2
    .arc = 0.2
    .windup = 0.15
    .recovery = 0.32
    .pattern = "slash"
weapon ForestSword from IronSword:
    .tint = vec3(0.55, 1, 0.7)
    .name = "Espada da floresta"
    .level = 2
    .rarity = "Incomum"
    .damage = 23
    .reach = 2.6
    .arc = 0.8
    .windup = 0.30
    .recovery = 0.5
    .pattern = "thrust"
weapon TrailKnife from IronSword:
    .name = "Lamina leve"
    .damage = 8
    .reach = 1.6
    .arc = 0.5
    .windup = 0.08
    .recovery = 0.18
    .visual_scale = vec3(0.03, 0.03, 0.03)
weapon HeavyBlade from IronSword:
    .name = "Espada pesada"
    .level = 3
    .rarity = "Incomum"
    .damage = 32
    .reach = 2.5
    .arc = -0.05
    .windup = 0.55
    .recovery = 0.7
    .visual_scale = vec3(0.065, 0.065, 0.065)
    .tint = vec3(0.8, 0.85, 1)

@Components
component Sentinel:
    .version = 1
    Defaults:
        .visual = ""
        .home = vec3(0, 1, 0)
        .timer = 0
        .winding = false
        .dead = false
        .flash = 0
register_component(Sentinel)
component GroundLoot:
    .version = 1
    Defaults:
        .item = ""
register_component(GroundLoot)

@Globals
hero = null
avatar = null
blade = null
state = null
map_open = false
inventory_open = false
notice = "Explore a floresta ao norte."
hud_time = 0
walk_time = 0
facing = vec3(0, 0, -1)
selected_item = 0
selected_destination = 0
reach_marks = []
impact_marks = []

func maximum(a, b):
    if a > b:
        return a
    return b

func countdown(value, delta):
    left = value - delta
    if left < 0.000000001:
        return 0
    return left

func distance_xz(a, b):
    dx = a.x - b.x
    dz = a.z - b.z
    return math_sqrt(dx * dx + dz * dz)

func direction(a, b):
    length = maximum(0.001, distance_xz(a, b))
    return vec3((b.x - a.x) / length, 0, (b.z - a.z) / length)

func weapon_definition():
    item = equipped_item()
    if item == null:
        return get_definition("Items", IronSword)
    spec = data_merge(get_definition("Items", item.definition), item.attributes)
    spec.level = item.level
    spec.rarity = item.rarity
    return weapon_visuals(spec)
