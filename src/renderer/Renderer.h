#pragma once
#include "core/Math.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>
namespace game {
struct Vertex { Vec3 position; Vec3 normal; Vec2 uv; };
struct MeshResource;
struct TextureResource;
using MeshHandle = std::shared_ptr<const MeshResource>;
using TextureHandle = std::shared_ptr<const TextureResource>;
enum class AlphaMode { Opaque, Mask };
struct Material {
    Color color{};
    TextureHandle texture;
    bool unlit = false;
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};
struct RenderItem { MeshHandle mesh; Material material; Matrix world; };
struct Camera { Vec3 eye{4,3,-6}, target{}; float nearPlane = 0.1f, farPlane = 200, fov = 0.8f; };
struct RenderConfig { bool vsync = true; float scale = 1; };
struct RenderStats { std::uint64_t draws = 0, triangles = 0; double cpuMilliseconds = 0; };
class Renderer {
public:
    Renderer(void* nativeWindow, int width, int height, const std::filesystem::path& shader, RenderConfig config = {});
    ~Renderer();
    MeshHandle createMesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices);
    TextureHandle createTexture(unsigned width, unsigned height, std::span<const std::uint8_t> rgba, bool srgb = true, bool nearest = true);
    void resize(int width, int height);
    void render(const Camera& camera, std::span<const RenderItem> items);
    void present();
    // Development readback of the internal target, before present.
    std::vector<std::uint8_t> readback();
    int renderWidth() const;
    int renderHeight() const;
    RenderStats stats() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
