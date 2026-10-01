@Components
component Visits:
    .version = 1
    Defaults:
        .count = 0
register_component(Visits)

@Entities
entity Marker:
    Assets:
        .model = "core:cube"

@Globals
marker = null

func half_damage(context):
    context.amount = context.amount / 2
    return context

add_hook("core:damage", half_damage)

@Events
on_start():
    marker = spawn(Marker, vec3(0, 0, 0))
    add_component(marker, Visits)
    set_health(marker, 100)
    log("E: save checkpoint; left mouse: load checkpoint")

on_update(delta):
    if action_pressed("Interact"):
        visits = get_component(marker, Visits)
        visits.count = visits.count + 1
        set_component(marker, Visits, visits)
        damage(marker, 10)
        save_game("checkpoint")
    if action_pressed("Attack"):
        load_game("checkpoint")

on_before_save():
    state = record()
    state.schema = 1
    state.message = "Checkpoint module state"
    set_save_state(state)

on_saved():
    log("Checkpoint saved")

on_loaded():
    marker = get_entity("checkpoint:Marker")
    log("Checkpoint restored; entity references refreshed")
