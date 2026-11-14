? Run standalone with --mods examples/shaders, or install alongside content.
@Entities
entity Preview:
    Assets:
        .model = "core:cube"

@Events
on_start():
    grade = record()
    grade.parameters = [1.05, 1.0, 0.95, 0.15]
    grade.priority = 10
    set_post_effect("shader_lab:grade", "shader_lab:grade", grade)
    preview = spawn(Preview, vec3(0, 0, 0))
    glow = record()
    glow.parameters = [0.25, 0.7, 1.0, 0.25, 2.0]
    set_entity_shader(preview, "shader_lab:glow", glow)
    log("Shader laboratory ready")
