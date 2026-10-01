? Stage 13 material preview. Display scales are not the final game scale.
@Entities
entity Sword {
    Assets:
        .model = "core:sword"
}
entity Tree {
    Assets:
        .model = "core:tree"
}
entity Waystone {
    Assets:
        .model = "core:waystone"
}

@Events
on_start():
    sword = spawn(Sword, vec3(0.3, -1.7, -1.8))
    set_scale(sword, vec3(0.15, 0.15, 0.15))
    tree = spawn(Tree, vec3(-1.7, 0, 1))
    set_scale(tree, vec3(0.025, 0.025, 0.025))
    waystone = spawn(Waystone, vec3(1.5, 0.04, 0.8))
    set_scale(waystone, vec3(0.04, 0.04, 0.04))
    log("Asset gallery ready: sword, tree, waystone")
