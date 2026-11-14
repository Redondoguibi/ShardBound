#pragma once
#include "assets/VirtualFileSystem.h"
#include "renderer/Shader.h"
#include <nlohmann/json.hpp>
namespace game {
struct ShaderSource { std::string id; std::filesystem::path source; ShaderKind kind=ShaderKind::Surface; };
ShaderSource shaderSource(const std::string& id,const nlohmann::json& entry);
std::string readShaderText(const std::filesystem::path& file);
CompiledShader compileShader(const VirtualFileSystem& vfs,const ShaderSource& source,const std::string& library);
}
