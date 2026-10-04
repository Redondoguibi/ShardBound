? A dependent mod changes data and damage behavior without changing C++.
@Items
modify "adventure:IronSword" {
    .damage = 18
    .reach = 2.4
}

func empowered_melee(hit):
    hit.amount = hit.amount + 2
    return hit

add_hook("adventure:melee", empowered_melee)

@Items
weapon TrailBlade from "adventure:IronSword":
    .name = "Lamina do caminho"
    .level = 3
    .rarity = "Incomum"
    .damage = 25
    .tint = vec3(0.6, 0.8, 1)

@Regions
region NorthTrail from "adventure:Forest":
    .name = "TRILHA DO NORTE"
    .min = vec3(-18, 0, 42)
    .max = vec3(18, 0, 62)
    .trees = [vec3(-8, 0, 50), vec3(8, 0, 50)]
    .enemies = []
    .shelters = []

@Waystones
waystone NorthCamp from "adventure:Grove":
    .label = "trailcraft_camp"
    .name = "Acampamento norte"
    .region = "trailcraft:NorthTrail"
    .position = vec3(0, 0.06, 55)
    .destination = vec3(0, 1.1, 53)

func customize_item(item):
    if item.definition == "trailcraft:TrailBlade":
        item.level = 4
        item.rarity = "Rara"
        item.attributes.damage = 31
    return item

add_hook("adventure:create_item", customize_item)

@Events
on_start():
    give_item(get_entity("player"), TrailBlade, 1)
