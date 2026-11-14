#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace game {
enum class ShaderKind { Surface, PostProcess };
struct ShaderResource;
using ShaderHandle = std::shared_ptr<const ShaderResource>;
struct CompiledShader {
    ShaderKind kind=ShaderKind::Surface;
    std::vector<std::uint8_t> pixel,shadow;
};
using ShaderParameters = std::array<float,16>;
}
