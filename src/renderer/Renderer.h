#pragma once
#include "core/Math.h"
#include "renderer/Shader.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>
namespace game {
struct Vertex { Vec3 position; Vec3 normal; Vec2 uv; Color color{}; };
struct MeshResource;
struct TextureResource;
using MeshHandle = std::shared_ptr<const MeshResource>;
using TextureHandle = std::shared_ptr<const TextureResource>;
struct ShaderInstance {
    ShaderHandle shader;
    ShaderParameters parameters{};
    TextureHandle texture;
    std::string shaderId,textureId;
};
using ShaderInstanceHandle = std::shared_ptr<const ShaderInstance>;
enum class AlphaMode { Opaque, Mask, Blend, Additive };
struct PointLight { Vec3 position; float range=1; Color color; float intensity=1; };
struct Material {
    Color color{};
    TextureHandle texture;
    bool unlit = false;
    bool surfaceOverlay = false;
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    ShaderInstanceHandle shader;
};
struct RenderItem { MeshHandle mesh; Material material; Matrix world; };
struct Camera { Vec3 eye{4,3,-6}, target{}; float nearPlane = 0.1f, farPlane = 200, fov = 0.8f; };
struct RenderConfig { bool vsync = true; float scale = 1; };
struct Environment {
    bool enabled=false, shadows=true;
    Color sky{0.25f,0.36f,0.4f,1}, sun{1.1f,0.95f,0.72f,1}, ambient{0.3f,0.4f,0.48f,1};
    float fogStart=24, fogEnd=70;
};
struct RenderStats { std::uint64_t draws = 0, triangles = 0, instances = 0, shadowDraws = 0, postDraws = 0; double cpuMilliseconds = 0,gpuMilliseconds=-1; };
class Renderer {
public:
    Renderer(void* nativeWindow, int width, int height, const std::filesystem::path& shader, RenderConfig config = {});
    ~Renderer();
    MeshHandle createMesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices);
    MeshHandle updateDynamicMesh(MeshHandle mesh, std::span<const Vertex> vertices, std::span<const std::uint32_t> indices);
    TextureHandle createTexture(unsigned width, unsigned height, std::span<const std::uint8_t> rgba, bool srgb = true, bool nearest = true);
    ShaderHandle createShader(const CompiledShader& code);
    const std::string& shaderLibrary() const;
    void setPostEffect(std::string id,ShaderInstanceHandle shader,int priority=0);
    void removePostEffect(const std::string& id);
    void setTextureShader(TextureHandle texture,ShaderInstanceHandle shader,const std::string& owner);
    void clearShaderEffects();
    void setShaderTime(double seconds);
    size_t postEffectCount() const;
    void resize(int width, int height);
    void render(const Camera& camera, std::span<const RenderItem> items,TextureHandle overlay = {},TextureHandle foreground = {},TextureHandle underlay = {});
    void present();
    // Development readback of the internal target, before present.
    std::vector<std::uint8_t> readback();
    int renderWidth() const;
    int renderHeight() const;
    RenderStats stats() const;
    std::string deviceName() const;
    void setEnvironment(const Environment& environment);
    Environment environment() const;
    std::vector<PointLight> pointLights() const;
    void scene(Environment environment,std::vector<PointLight> lights);
    void setCanvasOverlay(bool value);
    void setPointLights(std::span<const PointLight> lights);
    void configure(RenderConfig config);
    void setShadows(bool enabled);
    void performance(bool enabled);
    void uiScale(float scale);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
