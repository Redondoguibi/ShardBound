#include "renderer/Renderer.h"
#include "renderer/d3d11/D3DHelpers.h"
#include "core/Log.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
namespace game {
using namespace d3d;
struct MeshResource { ComPtr<ID3D11Buffer> vertices, indices; UINT count = 0; };
struct TextureResource { ComPtr<ID3D11ShaderResourceView> view; ComPtr<ID3D11SamplerState> sampler; };
struct Renderer::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap;
    ComPtr<ID3D11RenderTargetView> back, color;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11ShaderResourceView> targetView;
    ComPtr<ID3D11DepthStencilView> depth;
    ComPtr<ID3D11VertexShader> vertexShader, screenVertex;
    ComPtr<ID3D11PixelShader> pixelShader, screenPixel;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11SamplerState> screenSampler;
    TextureHandle white;
    RenderConfig config;
    RenderStats stats;
    int width = 0, height = 0, renderWidth = 0, renderHeight = 0;
    bool tearing = false;
    ~Impl() { if (context) { context->ClearState(); context->Flush(); } }
    void targets() {
        ComPtr<ID3D11Texture2D> buffer;
        check(swap->GetBuffer(0, IID_PPV_ARGS(&buffer)), "GetBuffer");
        check(device->CreateRenderTargetView(buffer.Get(), nullptr, &back), "backbuffer view");
        renderWidth = std::max(1, int(float(width) * config.scale));
        renderHeight = std::max(1, int(float(height) * config.scale));
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(renderWidth); desc.Height = UINT(renderHeight); desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        check(device->CreateTexture2D(&desc, nullptr, &target), "color target");
        check(device->CreateRenderTargetView(target.Get(), nullptr, &color), "color view");
        check(device->CreateShaderResourceView(target.Get(), nullptr, &targetView), "color shader view");
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> depthTexture;
        check(device->CreateTexture2D(&desc, nullptr, &depthTexture), "depth target");
        check(device->CreateDepthStencilView(depthTexture.Get(), nullptr, &depth), "depth view");
    }
};
Renderer::Renderer(void* nativeWindow, int width, int height, const std::filesystem::path& shader, RenderConfig config)
    : impl_(std::make_unique<Impl>()) {
    if (!nativeWindow || width <= 0 || height <= 0 || !std::isfinite(config.scale) || config.scale < 0.25f || config.scale > 1)
        throw std::invalid_argument("Invalid window or render scale (0.25..1)");
    auto& p = *impl_; p.config = config; p.width = width; p.height = height;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL selected{};
    auto create = [&](UINT value) { return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, value,
        levels, 2, D3D11_SDK_VERSION, &p.device, &selected, &p.context); };
    HRESULT result = create(flags);
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        LOG_WARN("RENDER", "Debug layer unavailable; creating hardware device without it");
        result = create(flags & ~D3D11_CREATE_DEVICE_DEBUG);
    }
    check(result, "device creation");
    ComPtr<IDXGIDevice> dxgi; check(p.device.As(&dxgi), "DXGI device");
    ComPtr<IDXGIAdapter> adapter; check(dxgi->GetAdapter(&adapter), "adapter");
    ComPtr<IDXGIFactory2> factory; check(adapter->GetParent(IID_PPV_ARGS(&factory)), "factory");
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory.As(&factory5))) {
        BOOL allow = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow)))) p.tearing = allow;
    }
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = UINT(width); desc.Height = UINT(height); desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; desc.Flags = p.tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    check(factory->CreateSwapChainForHwnd(p.device.Get(), static_cast<HWND>(nativeWindow), &desc, nullptr, nullptr, &p.swap), "flip swap chain");
    check(factory->MakeWindowAssociation(static_cast<HWND>(nativeWindow), DXGI_MWA_NO_ALT_ENTER), "window association");
    p.targets();
    auto compile = [&](const char* entry, const char* profile) {
        ComPtr<ID3DBlob> bytecode, errors;
        const auto status = D3DCompileFromFile(shader.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, profile,
            D3DCOMPILE_ENABLE_STRICTNESS, 0, &bytecode, &errors);
        if (errors) LOG_WARN("SHADER", std::string_view(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
        check(status, entry); return bytecode;
    };
    auto vs = compile("vertexMain", "vs_5_0"), ps = compile("pixelMain", "ps_5_0");
    check(p.device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &p.vertexShader), "vertex shader");
    check(p.device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &p.pixelShader), "pixel shader");
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    check(p.device->CreateInputLayout(layout, 3, vs->GetBufferPointer(), vs->GetBufferSize(), &p.layout), "input layout");
    vs = compile("screenVertex", "vs_5_0"); ps = compile("screenPixel", "ps_5_0");
    check(p.device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &p.screenVertex), "screen vertex shader");
    check(p.device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &p.screenPixel), "screen pixel shader");
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = 144; cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(p.device->CreateBuffer(&cb, nullptr, &p.constants), "constant buffer");
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
    check(p.device->CreateRasterizerState(&raster, &p.raster), "raster state");
    D3D11_SAMPLER_DESC sampler{}; sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sampler.MaxLOD = D3D11_FLOAT32_MAX;
    check(p.device->CreateSamplerState(&sampler, &p.screenSampler), "screen sampler");
    const std::uint8_t white[] = {255,255,255,255}; p.white = createTexture(1,1,white);
    LOG_INFO("RENDER", std::format("D3D11 initialized, feature level {:X}, flip-model, {}x{}", unsigned(selected), width, height));
}
Renderer::~Renderer() = default;
MeshHandle Renderer::createMesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty() || indices.size() % 3 || vertices.size_bytes() > UINT_MAX || indices.size_bytes() > UINT_MAX)
        throw std::invalid_argument("Invalid mesh buffer size");
    for (auto index : indices) if (index >= vertices.size()) throw std::invalid_argument("Mesh index out of bounds");
    auto mesh = std::make_shared<MeshResource>();
    D3D11_BUFFER_DESC desc{}; desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; desc.ByteWidth = UINT(vertices.size_bytes());
    D3D11_SUBRESOURCE_DATA data{}; data.pSysMem = vertices.data();
    check(impl_->device->CreateBuffer(&desc, &data, &mesh->vertices), "vertex buffer");
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER; desc.ByteWidth = UINT(indices.size_bytes()); data.pSysMem = indices.data();
    check(impl_->device->CreateBuffer(&desc, &data, &mesh->indices), "index buffer");
    mesh->count = UINT(indices.size()); return mesh;
}
TextureHandle Renderer::createTexture(unsigned width, unsigned height, std::span<const std::uint8_t> rgba, bool srgb, bool nearest) {
    if (!width || !height || width > 16384 || height > 16384 || rgba.size() != size_t(width)*height*4)
        throw std::invalid_argument("Invalid RGBA texture dimensions");
    auto texture = std::make_shared<TextureResource>();
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{}; data.pSysMem = rgba.data(); data.SysMemPitch = width * 4;
    ComPtr<ID3D11Texture2D> gpu;
    check(impl_->device->CreateTexture2D(&desc, &data, &gpu), "texture upload");
    check(impl_->device->CreateShaderResourceView(gpu.Get(), nullptr, &texture->view), "texture view");
    D3D11_SAMPLER_DESC sampler{}; sampler.Filter = nearest ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP; sampler.MaxLOD = D3D11_FLOAT32_MAX;
    check(impl_->device->CreateSamplerState(&sampler, &texture->sampler), "texture sampler"); return texture;
}
void Renderer::resize(int width, int height) {
    auto& p = *impl_;
    if (width <= 0 || height <= 0 || (width == p.width && height == p.height)) return;
    p.context->ClearState(); p.back.Reset(); p.color.Reset(); p.depth.Reset(); p.targetView.Reset(); p.target.Reset();
    check(p.swap->ResizeBuffers(0, UINT(width), UINT(height), DXGI_FORMAT_UNKNOWN, p.tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0), "resize");
    p.width = width; p.height = height; p.targets();
}
void Renderer::render(const Camera& camera, std::span<const RenderItem> items) {
    using namespace DirectX;
    const auto start = std::chrono::steady_clock::now();
    auto& p = *impl_; p.stats = {};
    const float clear[] = {0.025f,0.035f,0.06f,1};
    ID3D11ShaderResourceView* none = nullptr; p.context->PSSetShaderResources(0,1,&none);
    p.context->OMSetRenderTargets(1,p.color.GetAddressOf(),p.depth.Get());
    p.context->ClearRenderTargetView(p.color.Get(), clear); p.context->ClearDepthStencilView(p.depth.Get(),D3D11_CLEAR_DEPTH,1,0);
    D3D11_VIEWPORT viewport{0,0,float(p.renderWidth),float(p.renderHeight),0,1}; p.context->RSSetViewports(1,&viewport);
    p.context->RSSetState(p.raster.Get());
    p.context->IASetInputLayout(p.layout.Get()); p.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    p.context->VSSetShader(p.vertexShader.Get(),nullptr,0); p.context->PSSetShader(p.pixelShader.Get(),nullptr,0);
    p.context->VSSetConstantBuffers(0,1,p.constants.GetAddressOf()); p.context->PSSetConstantBuffers(0,1,p.constants.GetAddressOf());
    const auto view = XMMatrixLookAtLH(XMVectorSet(camera.eye.x,camera.eye.y,camera.eye.z,1), XMVectorSet(camera.target.x,camera.target.y,camera.target.z,1), XMVectorSet(0,1,0,0));
    const auto projection = XMMatrixPerspectiveFovLH(camera.fov,float(p.renderWidth)/float(p.renderHeight),camera.nearPlane,camera.farPlane);
    struct Constants { XMFLOAT4X4 mvp, world; Color color; } constants;
    for (const auto& item : items) {
        if (!item.mesh) continue;
        XMFLOAT4X4 world; std::copy_n(item.world.values.begin(),16,&world._11);
        const auto matrix = XMLoadFloat4x4(&world);
        XMStoreFloat4x4(&constants.mvp,XMMatrixTranspose(matrix * view * projection));
        XMStoreFloat4x4(&constants.world,XMMatrixTranspose(matrix)); constants.color = item.material.color;
        constants.color.a = item.material.unlit ? 0.0f : 1.0f;
        p.context->UpdateSubresource(p.constants.Get(),0,nullptr,&constants,0,0);
        UINT stride = sizeof(Vertex), offset = 0;
        p.context->IASetVertexBuffers(0,1,item.mesh->vertices.GetAddressOf(),&stride,&offset);
        p.context->IASetIndexBuffer(item.mesh->indices.Get(),DXGI_FORMAT_R32_UINT,0);
        const auto& texture = item.material.texture ? item.material.texture : p.white;
        p.context->PSSetShaderResources(0,1,texture->view.GetAddressOf()); p.context->PSSetSamplers(0,1,texture->sampler.GetAddressOf());
        p.context->DrawIndexed(item.mesh->count,0,0); ++p.stats.draws; p.stats.triangles += item.mesh->count/3;
    }
    p.context->OMSetRenderTargets(1,p.back.GetAddressOf(),nullptr);
    viewport.Width = float(p.width); viewport.Height = float(p.height); p.context->RSSetViewports(1,&viewport);
    p.context->IASetInputLayout(nullptr); p.context->VSSetShader(p.screenVertex.Get(),nullptr,0); p.context->PSSetShader(p.screenPixel.Get(),nullptr,0);
    p.context->PSSetShaderResources(0,1,p.targetView.GetAddressOf()); p.context->PSSetSamplers(0,1,p.screenSampler.GetAddressOf());
    p.context->Draw(3,0); p.context->PSSetShaderResources(0,1,&none);
    p.stats.cpuMilliseconds = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
void Renderer::present() {
    auto& p = *impl_;
    const auto result = p.swap->Present(p.config.vsync ? 1 : 0, !p.config.vsync && p.tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
    if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) check(p.device->GetDeviceRemovedReason(), "device removed");
    check(result, "present");
}
std::vector<std::uint8_t> Renderer::readback() {
    auto& p = *impl_; D3D11_TEXTURE2D_DESC desc{}; p.target->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging; check(p.device->CreateTexture2D(&desc,nullptr,&staging), "readback texture");
    p.context->CopyResource(staging.Get(),p.target.Get()); D3D11_MAPPED_SUBRESOURCE map{};
    check(p.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map), "readback map");
    std::vector<std::uint8_t> pixels(size_t(desc.Width)*desc.Height*4);
    for (UINT y=0;y<desc.Height;++y) std::memcpy(pixels.data()+size_t(y)*desc.Width*4, static_cast<const std::uint8_t*>(map.pData)+size_t(y)*map.RowPitch,size_t(desc.Width)*4);
    p.context->Unmap(staging.Get(),0); return pixels;
}
int Renderer::renderWidth() const { return impl_->renderWidth; }
int Renderer::renderHeight() const { return impl_->renderHeight; }
RenderStats Renderer::stats() const { return impl_->stats; }
}
