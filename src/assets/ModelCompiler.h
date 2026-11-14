#pragma once
#include "assets/TextureCompiler.h"
#include "renderer/Renderer.h"
#include "assets/ModelAnimation.h"
namespace game {
struct ModelPartData {
    size_t node=0;
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Matrix transform;
    Color color;
    TextureData texture;
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};
struct ModelData { std::vector<ModelPartData> parts; bool cacheHit = false; ModelHierarchy hierarchy; };
ModelData compileModel(const std::filesystem::path& source,const std::filesystem::path& cache);
}
