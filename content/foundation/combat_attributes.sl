? This contract is imported by the combat hook independently of the player lifecycle.
@Globals
damage_attribute = record()
damage_attribute.default = 1
damage_attribute.min = 0
damage_attribute.max = 100
register_attribute("foundation:damage_multiplier", damage_attribute)
armor_attribute = record()
armor_attribute.default = 0
armor_attribute.min = 0
armor_attribute.max = 1000000
register_attribute("foundation:armor_bonus", armor_attribute)
