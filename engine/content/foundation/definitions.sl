@Entities
entity Actor:
    .max_health = 100
    .speed = 5
    .jump = 7.2
    .body = vec3(0.32, 0.9, 0.32)
    .gravity = 1.5
entity Avatar:
    Assets:
        .model = "foundation:player"
entity Blade:
    Assets:
        .model = "foundation:sword"
entity Enemy:
    .body = vec3(0.35, 0.85, 0.35)
    .health = 40
    .speed = 2
    .damage = 9
    .reach = 1.8
    .aggro = 9
    .windup = 0.7
    .recovery = 1.1
    .gold = 8
    .drop = "foundation:ForestSword"
entity Scout from Enemy:
    .visual = "foundation:ScoutVisual"
entity Brute from Enemy:
    .visual = "foundation:BruteVisual"
    .health = 95
    .speed = 1.2
    .damage = 22
    .windup = 1.1
    .gold = 24
    .drop = "foundation:HeavyBlade"
entity ScoutVisual:
    Assets:
        .model = "foundation:scout"
entity BruteVisual:
    Assets:
        .model = "foundation:brute"
entity Drop:
    Assets:
        .model = "foundation:sword"

@Items
weapon IronSword:
    .name = "Espada de ferro"
    .model = "foundation:sword"
    .scale = vec3(0.05, 0.05, 0.05)
    .damage = 15
    .reach = 2.4
    .windup = 0.16
    .recovery = 0.34
    .rarity = "Comum"
weapon ForestSword from IronSword:
    .name = "Espada da planicie"
    .damage = 23
    .windup = 0.23
    .rarity = "Incomum"
weapon HeavyBlade from IronSword:
    .name = "Espada pesada"
    .damage = 32
    .windup = 0.45
    .recovery = 0.7
    .reach = 2.7

@Rules
rules Gameplay:
    .starting_weapon = "foundation:IronSword"
    .death_gold_loss = 0.15
    .falling_critical = 1.7
    .encounter_radius = 1
    .despawn_distance = 45

@Components
component Brain:
    .version = 1
    Defaults:
        .key = ""
        .visual = ""
        .timer = 0
        .winding = false
register_component(Brain)
component GroundDrop:
    .version = 1
    Defaults:
        .item = ""
register_component(GroundDrop)

@Globals
hero = null
avatar = null
blade = null
state = null
attack_time = 0
winding = false
attack_profile = null
combo_index = 0
combo_time = 0
combo_aerial = false
facing = vec3(0, 0, 1)
inventory_open = false
selected = 0
hud_timer = 0
walk_time = 0
encounter_timer = 0
notice = "Explore as planicies."
canvas = []
