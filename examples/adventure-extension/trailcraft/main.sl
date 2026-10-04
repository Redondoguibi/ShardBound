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
