func movement(context):
    actor = get_entity_by_key(context.player_key)
    options = record()
    if actor != null and not is_grounded(actor):
        options.right = context.right * 0.6
        options.forward = context.forward * 0.6
    engine_default(options)
    return true

func player_light(context):
    actor = get_entity("player")
    if actor == null:
        return
    light = record()
    light.position = vec3(actor.position.x, actor.position.y + 3, actor.position.z)
    light.color = vec3(1, 0.7, 0.4)
    light.range = 7
    light.intensity = 1.5
    scene = record()
    scene.lights = [light]
    configure_engine("render", scene)

@Events
on_start():
    physics = record()
    physics.gravity = 3
    physics.max_slope = 45
    configure_engine("physics", physics)
    scheduler = record()
    scheduler.fixed_order = ["movement", "fixed", "physics"]
    configure_engine("scheduler", scheduler)
    options = record()
    options.api = 1
    options.mode = "replace"
    options.replaces = "core:default"
    register_engine_extension("low_gravity:movement", "movement", movement, options)
    register_engine_extension("low_gravity:lighting", "render", player_light)
