import "scripts/entities.sl"

@Globals
cube = null
direction = 1

@Events
on_start():
    cube = spawn(Cube, vec3(0, 0, 0))

on_update(delta):
    cube.position.x = cube.position.x + direction * delta
    if cube.position.x > 1.5:
        direction = -1
    if cube.position.x < -1.5:
        direction = 1
