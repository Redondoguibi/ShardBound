? Technical sandbox; values demonstrate APIs, not final game balance.
@Entities
entity Player {
    Assets:
        .model = "sandbox:player"
}
entity Box {
    Assets:
        .model = "core:cube"
}

@Items
item Token {
    Attributes:
        .name = "Sandbox token"
}

@Globals
player = null
target = null

@Events
on_start():
    floor = spawn(Box, vec3(0, -0.5, 0))
    set_scale(floor, vec3(10, 0.5, 10))
    add_box(floor, vec3(10, 0.5, 10), false)
    player = spawn(Player, vec3(0, 1.2, 0))
    add_box(player, vec3(0.5, 1, 0.25), true)
    set_health(player, 100)
    set_player(player, 4)
    target = spawn(Box, vec3(0, 0.5, 3))
    set_scale(target, vec3(0.5, 0.5, 0.5))
    add_box(target, vec3(0.5, 0.5, 0.5), false)
    set_health(target, 30)
    set_interactable(target, true)
    log("WASD/gamepad: move; E: collect token; left mouse: damage nearby target")

on_update(delta):
    if action_pressed("Interact"):
        nearby = nearest_interactable(player, 2)
        if nearby != null:
            give_item(player, Token, 1)
            play_sound("sandbox:hit")
            log("Token added to inventory")
    if action_pressed("Attack"):
        nearby = nearest_interactable(player, 2)
        if nearby != null:
            damage(nearby, 10)
            play_sound("sandbox:hit")
            if health(nearby) == 0:
                destroy(nearby)
                log("Target defeated")
