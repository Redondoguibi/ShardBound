? Provisional tuning. Definitions are public and patchable by dependent mods.
@Entities
entity Actor:
    .max_health = 100
    .speed = 4
    .jump = 6
entity Enemy:
    .max_health = 45
    .speed = 1.8
    .damage = 12
    .windup = 0.65
    .recovery = 1.2
    .reward = 12
entity Explorer:
    Assets:
        .model = "core:player"
entity Block:
    Assets:
        .model = "core:solid"
entity Tree:
    Assets:
        .model = "core:tree"
entity Waystone:
    Assets:
        .model = "core:waystone"
entity Blade:
    Assets:
        .model = "core:sword"

@Items
weapon IronSword:
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
    .name = "Espada da floresta"
    .level = 2
    .rarity = "Incomum"
    .damage = 23
    .reach = 2.6
    .arc = 0.8
    .windup = 0.30
    .recovery = 0.5
    .pattern = "thrust"

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
    return get_definition("Items", state.equipped)
