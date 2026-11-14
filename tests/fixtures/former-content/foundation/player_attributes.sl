? Attribute IDs and their gameplay effects belong to this mod, not the engine.
func define_player_attribute(id, field, minimum, maximum):
    spec = record()
    spec.default = record()
    spec.default.category = "Entities"
    spec.default.id = "foundation:Actor"
    spec.default.field = field
    spec.min = minimum
    spec.max = maximum
    register_attribute(id, spec)

@Globals
define_player_attribute("foundation:movement_speed", "speed", 0.1, 100)
define_player_attribute("foundation:jump_speed", "jump", 0.1, 100)
define_player_attribute("foundation:max_health", "max_health", 1, 1000000)
applied_movement_speed = 0

func apply_player_attributes():
    if hero == null:
        return
    speed = get_attribute(hero, "foundation:movement_speed")
    if speed != applied_movement_speed:
        set_player(hero, speed)
        applied_movement_speed = speed
    set_maximum_health(hero, get_attribute(hero, "foundation:max_health"))

@Events
on_attribute_changed(actor, id, before, after):
    if actor == hero:
        apply_player_attributes()

on_item_durability(actor, instance, before, after):
    if actor == hero:
        apply_player_attributes()
