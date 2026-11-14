import "combat_attributes.sl"
? Combat stat policy belongs to the gameplay mod. The engine only aggregates bonuses.
func equipment_damage(context):
    amount = context.amount
    if context.source != null:
        weapon = equipped_instance(context.source, "weapon")
        if weapon != null and item_instance(context.source, weapon).durability <= 0:
            context.amount = 0
            return context
        amount = equipment_stat(context.source, "damage", amount)
        amount = amount * get_attribute(context.source, "foundation:damage_multiplier")
    armor = equipment_stat(context.target, "armor", 0)
    armor = armor + get_attribute(context.target, "foundation:armor_bonus")
    if armor > 0:
        amount = amount - armor
    if amount < 0:
        amount = 0
    context.amount = amount
    return context

@Globals
equipment_damage_hook = add_hook("core:damage", equipment_damage)

@Events
on_damage_dealt(source, target, amount):
    if amount > 0:
        weapon = equipped_instance(source, "weapon")
        if weapon != null:
            remaining = item_instance(source, weapon).durability - 1
            if remaining < 0:
                remaining = 0
            item_durability(source, weapon, remaining)
