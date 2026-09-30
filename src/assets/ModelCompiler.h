#pragma once
#include "assets/TextureCompiler.h"
#include "renderer/Renderer.h"
namespace game {
struct ModelPartData {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Matrix transform;
    Color color;
    TextureData texture;
};
struct ModelData { std::vector<ModelPartData> parts; bool cacheHit = false; };
ModelData compileModel(const std::filesystem::path& source,const std::filesystem::path& cache);
}
