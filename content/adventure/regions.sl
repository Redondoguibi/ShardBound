func encounter(key, definition, where):
    entry = record()
    entry.key = key
    entry.definition = definition
    entry.position = where
    return entry

@Regions
region Hub:
    .name = "ABRIGO"
    .priority = 10
    .safe = true
    .min = vec3(-18, 0, -14)
    .max = vec3(18, 0, 7)
    .color = vec3(0.62, 0.54, 0.37)
    .trees = []
    .shelters = [vec3(-7, 0, -3), vec3(7, 0, -3), vec3(-7, 0, 4)]
    .enemies = []
    .encounters = []
region Forest:
    .name = "FLORESTA"
    .priority = 0
    .safe = false
    .min = vec3(-18, 0, 7)
    .max = vec3(18, 0, 42)
    .color = vec3(0.3, 0.48, 0.25)
    .trees = [vec3(-13, 0, 10), vec3(-8, 0, 10), vec3(8, 0, 10), vec3(13, 0, 10), vec3(-13, 0, 17), vec3(-8, 0, 17), vec3(8, 0, 17), vec3(13, 0, 17), vec3(-13, 0, 24), vec3(-8, 0, 24), vec3(8, 0, 24), vec3(13, 0, 24), vec3(-13, 0, 32), vec3(-8, 0, 32), vec3(8, 0, 32), vec3(13, 0, 32), vec3(-13, 0, 39), vec3(-8, 0, 39), vec3(8, 0, 39), vec3(13, 0, 39)]
    .shelters = []
    .enemies = [vec3(2, 1, 14), vec3(-3, 1, 26), vec3(4, 1, 31)]
    .encounters = [encounter("adventure:ScoutWest", "adventure:Scout", vec3(-5, 0, 18)), encounter("adventure:BruteEast", "adventure:Brute", vec3(5, 0, 28))]

@Waystones
waystone Hub:
    .label = "hub_waystone"
    .name = "Abrigo"
    .region = "adventure:Hub"
    .position = vec3(3, 0.06, 0)
    .destination = vec3(3, 1.1, -2)
    .radius = 2.5
waystone Forest:
    .label = "forest_waystone"
    .name = "Floresta"
    .region = "adventure:Forest"
    .position = vec3(-3, 0.06, 20)
    .destination = vec3(-3, 1.1, 18)
    .radius = 2.5
waystone Grove:
    .label = "grove_waystone"
    .name = "Clareira"
    .region = "adventure:Forest"
    .position = vec3(3, 0.06, 36)
    .destination = vec3(3, 1.1, 34)
    .radius = 2.5

@Worlds
world Main:
    .start = vec3(0, 1.1, 0)
    .respawn = vec3(0, 1.1, 0)

@Globals
catalog_revision = -1
region_catalog = []
destination_catalog = []

func refresh_world_catalogs():
    revision = registry_revision()
    if catalog_revision == revision:
        return
    region_catalog = get_definitions("Regions")
    destination_catalog = []
    ? Preserve the original three shortcuts, then append all mod destinations.
    for id in ["adventure:Hub", "adventure:Forest", "adventure:Grove"]:
        if definition_exists("Waystones", id):
            entry = record()
            entry.id = id
            entry.data = get_definition("Waystones", id)
            list_append(destination_catalog, entry)
    for entry in get_definitions("Waystones"):
        if entry.id != "adventure:Hub" and entry.id != "adventure:Forest" and entry.id != "adventure:Grove":
            list_append(destination_catalog, entry)
    catalog_revision = revision

func current_region():
    refresh_world_catalogs()
    found = null
    priority = -1000000
    for region in region_catalog:
        spec = region.data
        if hero.position.x >= spec.min.x and hero.position.x <= spec.max.x and hero.position.z >= spec.min.z and hero.position.z <= spec.max.z:
            if spec.priority > priority:
                found = spec
                priority = spec.priority
    return found

func in_safe_region():
    region = current_region()
    return region != null and region.safe

func discovery(id):
    for entry in state.discoveries:
        if entry.id == id:
            return entry
    entry = record()
    entry.id = id
    entry.active = false
    list_append(state.discoveries, entry)
    return list_at(state.discoveries, list_count(state.discoveries) - 1)

func discovered(id):
    for entry in state.discoveries:
        if entry.id == id:
            return entry.active
    return false

func destination_list():
    refresh_world_catalogs()
    return destination_catalog

func validate_world_content():
    refresh_world_catalogs()
    stones = destination_catalog
    if list_count(stones) > 63:
        script_error("At most 63 waystones are supported by this map")
    for entry in stones:
        spec = entry.data
        if spec.radius <= 0 or spec.radius > 100 or not definition_exists("Regions", spec.region):
            script_error("Invalid waystone radius or region")
        for other in stones:
            if entry.id != other.id and spec.label == other.data.label:
                script_error("Waystone entity labels must be unique")
    regions = region_catalog
    if list_count(regions) > 256:
        script_error("At most 256 regions are supported")
    encounter_keys = []
    for entry in regions:
        spec = entry.data
        if spec.min.x >= spec.max.x or spec.min.z >= spec.max.z:
            script_error("Invalid region bounds")
        if record_has(spec, "encounters"):
            for entry in spec.encounters:
                if text_length(entry.key) == 0 or text_length(entry.key) > 128 or not definition_exists("Entities", entry.definition):
                    script_error("Invalid encounter identity or definition")
                if list_contains(encounter_keys, entry.key):
                    script_error("Encounter keys must be unique across regions")
                list_append(encounter_keys, entry.key)
    if list_count(encounter_keys) > 256:
        script_error("At most 256 keyed encounters are supported")
