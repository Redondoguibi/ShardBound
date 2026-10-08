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
#include <map>
#include <tuple>
namespace game {
using namespace d3d;
struct MeshResource { ComPtr<ID3D11Buffer> vertices, indices; UINT count = 0; Vec3 center; float radius=0; };
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
    ComPtr<ID3D11PixelShader> pixelShader, screenPixel, shadowPixel;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> constants, instances;
    UINT instanceCapacity=0;
    ComPtr<ID3D11DepthStencilView> shadowDepth;
    ComPtr<ID3D11ShaderResourceView> shadowView;
    ComPtr<ID3D11SamplerState> shadowSampler;
    ComPtr<ID3D11RasterizerState> shadowRaster;
    ComPtr<ID3D11BlendState> overlayBlend;
    Environment environment;
    bool canvasOverlay=false;
    ComPtr<ID3D11RasterizerState> raster, rasterDoubleMirrored, rasterBack, rasterMirrored;
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
    std::vector<D3D11_INPUT_ELEMENT_DESC> layout{
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    for(UINT row=0;row<4;++row) layout.push_back({"WORLD",row,DXGI_FORMAT_R32G32B32A32_FLOAT,1,row*16,D3D11_INPUT_PER_INSTANCE_DATA,1});
    for(UINT row=0;row<4;++row) layout.push_back({"INORMAL",row,DXGI_FORMAT_R32G32B32A32_FLOAT,1,64+row*16,D3D11_INPUT_PER_INSTANCE_DATA,1});
    layout.push_back({"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,128,D3D11_INPUT_PER_INSTANCE_DATA,1});
    layout.push_back({"PARAMS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,144,D3D11_INPUT_PER_INSTANCE_DATA,1});
    check(p.device->CreateInputLayout(layout.data(),UINT(layout.size()),vs->GetBufferPointer(),vs->GetBufferSize(),&p.layout),"instanced input layout");
    ps=compile("shadowPixel","ps_5_0");
    check(p.device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&p.shadowPixel),"shadow pixel shader");
    vs = compile("screenVertex", "vs_5_0"); ps = compile("screenPixel", "ps_5_0");
    check(p.device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &p.screenVertex), "screen vertex shader");
    check(p.device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &p.screenPixel), "screen pixel shader");
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = 224; cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(p.device->CreateBuffer(&cb, nullptr, &p.constants), "constant buffer");
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
    raster.FrontCounterClockwise = FALSE;
    check(p.device->CreateRasterizerState(&raster, &p.raster), "raster state");
    raster.FrontCounterClockwise = TRUE;
    check(p.device->CreateRasterizerState(&raster, &p.rasterDoubleMirrored), "mirrored double-sided raster state");
    raster.CullMode = D3D11_CULL_BACK; raster.FrontCounterClockwise = FALSE;
    check(p.device->CreateRasterizerState(&raster, &p.rasterBack), "back-face raster state");
    raster.FrontCounterClockwise = TRUE;
    check(p.device->CreateRasterizerState(&raster, &p.rasterMirrored), "mirrored raster state");
    D3D11_SAMPLER_DESC sampler{}; sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sampler.MaxLOD = D3D11_FLOAT32_MAX;
    check(p.device->CreateSamplerState(&sampler, &p.screenSampler), "screen sampler");
    D3D11_TEXTURE2D_DESC shadow{}; shadow.Width=shadow.Height=2048; shadow.MipLevels=shadow.ArraySize=1;
    shadow.Format=DXGI_FORMAT_R32_TYPELESS; shadow.SampleDesc.Count=1;
    shadow.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> shadowTexture; check(p.device->CreateTexture2D(&shadow,nullptr,&shadowTexture),"shadow map");
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv{}; dsv.Format=DXGI_FORMAT_D32_FLOAT; dsv.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    check(p.device->CreateDepthStencilView(shadowTexture.Get(),&dsv,&p.shadowDepth),"shadow depth view");
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format=DXGI_FORMAT_R32_FLOAT; srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D; srv.Texture2D.MipLevels=1;
    check(p.device->CreateShaderResourceView(shadowTexture.Get(),&srv,&p.shadowView),"shadow resource view");
    sampler.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; sampler.ComparisonFunc=D3D11_COMPARISON_LESS_EQUAL;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;
    std::fill_n(sampler.BorderColor,4,1.0f);
    check(p.device->CreateSamplerState(&sampler,&p.shadowSampler),"shadow comparison sampler");
    raster.CullMode=D3D11_CULL_NONE; raster.FrontCounterClockwise=FALSE; raster.DepthBias=500; raster.SlopeScaledDepthBias=1.5f;
    check(p.device->CreateRasterizerState(&raster,&p.shadowRaster),"shadow raster");
    D3D11_BLEND_DESC blend{}; auto& channel=blend.RenderTarget[0]; channel.BlendEnable=TRUE;
    channel.SrcBlend=D3D11_BLEND_SRC_ALPHA; channel.DestBlend=D3D11_BLEND_INV_SRC_ALPHA; channel.BlendOp=D3D11_BLEND_OP_ADD;
    channel.SrcBlendAlpha=D3D11_BLEND_ONE; channel.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; channel.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    channel.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    check(p.device->CreateBlendState(&blend,&p.overlayBlend),"overlay alpha blend");
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
    mesh->count = UINT(indices.size());
    Vec3 low=vertices.front().position,high=low;
    for(const auto& v:vertices) { low.x=std::min(low.x,v.position.x); low.y=std::min(low.y,v.position.y); low.z=std::min(low.z,v.position.z);
        high.x=std::max(high.x,v.position.x); high.y=std::max(high.y,v.position.y); high.z=std::max(high.z,v.position.z); }
    mesh->center={(low.x+high.x)*.5f,(low.y+high.y)*.5f,(low.z+high.z)*.5f};
    mesh->radius=std::sqrt((high.x-low.x)*(high.x-low.x)+(high.y-low.y)*(high.y-low.y)+(high.z-low.z)*(high.z-low.z))*.5f;
    return mesh;
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
void Renderer::setEnvironment(const Environment& environment) { impl_->environment=environment; }
void Renderer::setCanvasOverlay(bool value) { impl_->canvasOverlay=value; }
void Renderer::render(const Camera& camera,std::span<const RenderItem> items,TextureHandle overlay) {
    using namespace DirectX;
    const auto start=std::chrono::steady_clock::now(); auto& p=*impl_; p.stats={};
    const auto& environment=p.environment;
    const auto view=XMMatrixLookAtLH(XMVectorSet(camera.eye.x,camera.eye.y,camera.eye.z,1),XMVectorSet(camera.target.x,camera.target.y,camera.target.z,1),XMVectorSet(0,1,0,0));
    const float aspect=float(p.renderWidth)/float(p.renderHeight), tangent=std::tan(camera.fov*.5f);
    const auto projection=XMMatrixPerspectiveFovLH(camera.fov,aspect,camera.nearPlane,camera.farPlane);
    const auto sunDirection=XMVector3Normalize(XMVectorSet(-.5f,1,-.6f,0));
    const auto center=XMVectorSet(camera.target.x,0,camera.target.z,1);
    const auto lightView=XMMatrixLookAtLH(XMVectorAdd(center,XMVectorScale(sunDirection,60)),center,XMVectorSet(0,1,0,0));
    const auto lightVP=lightView*XMMatrixOrthographicLH(60,60,.1f,140);
    struct Frame { XMFLOAT4X4 viewProjection,lightViewProjection; XMFLOAT4 sunDirection; Color sun,ambient,fog; XMFLOAT4 eyeFogStart,effects; } frame{};
    static_assert(sizeof(Frame)==224);
    XMStoreFloat4x4(&frame.lightViewProjection,XMMatrixTranspose(lightVP)); XMStoreFloat4(&frame.sunDirection,sunDirection);
    frame.sun=environment.sun; frame.ambient=environment.ambient; frame.fog=environment.sky;
    frame.eyeFogStart={camera.eye.x,camera.eye.y,camera.eye.z,environment.fogStart};
    frame.effects={environment.fogEnd,environment.enabled && environment.shadows ? 1.0f : 0.0f,environment.enabled ? 1.0f : 0.0f,0};
    struct Instance { XMFLOAT4X4 world,normal; Color color; XMFLOAT4 material; };
    static_assert(sizeof(Instance)==160);
    struct Batch { MeshHandle mesh; TextureHandle texture; bool twoSided=false,mirrored=false; std::vector<Instance> data; UINT offset=0; };
    using Key=std::tuple<std::uintptr_t,std::uintptr_t,bool,bool>;
    std::map<Key,Batch> visible,casters;
    auto append=[&](auto& batches,const RenderItem& item,const TextureHandle& texture,bool mirrored,const Instance& value) {
        auto& batch=batches[{reinterpret_cast<std::uintptr_t>(item.mesh.get()),reinterpret_cast<std::uintptr_t>(texture.get()),item.material.doubleSided,mirrored}];
        batch.mesh=item.mesh; batch.texture=texture; batch.twoSided=item.material.doubleSided; batch.mirrored=mirrored; batch.data.push_back(value);
    };
    for(const auto& item:items) {
        if(!item.mesh) continue;
        Instance value{}; std::copy_n(item.world.values.begin(),16,&value.world._11);
        const auto matrix=XMLoadFloat4x4(&value.world); const auto determinant=XMVectorGetX(XMMatrixDeterminant(matrix));
        XMStoreFloat4x4(&value.normal,std::abs(determinant)>1e-12f ? XMMatrixTranspose(XMMatrixInverse(nullptr,matrix)) : matrix);
        value.color=item.material.color;
        value.material={item.material.alphaCutoff,item.material.alphaMode==AlphaMode::Mask ? 1.0f : 0.0f,item.material.unlit ? 1.0f : 0.0f,item.material.doubleSided ? 1.0f : 0.0f};
        const auto& texture=item.material.texture ? item.material.texture : p.white;
        const auto localCenter=item.mesh->center;
        const auto worldCenter=XMVector3TransformCoord(XMVectorSet(localCenter.x,localCenter.y,localCenter.z,1),matrix);
        XMFLOAT3 position; XMStoreFloat3(&position,XMVector3TransformCoord(worldCenter,view));
        const float radius=item.mesh->radius*std::sqrt(XMVectorGetX(XMVector3LengthSq(matrix.r[0]))+XMVectorGetX(XMVector3LengthSq(matrix.r[1]))+XMVectorGetX(XMVector3LengthSq(matrix.r[2])));
        if(position.z+radius>=camera.nearPlane && position.z-radius<=camera.farPlane &&
            std::abs(position.x)<=position.z*tangent*aspect+radius*std::sqrt(1+tangent*tangent*aspect*aspect) &&
            std::abs(position.y)<=position.z*tangent+radius*std::sqrt(1+tangent*tangent)) append(visible,item,texture,determinant<0,value);
        if(frame.effects.y!=0 && !item.material.unlit && XMVectorGetX(XMVector3Length(XMVectorSubtract(worldCenter,center)))<50+radius)
            append(casters,item,texture,determinant<0,value);
    }
    std::vector<Instance> data;
    for(auto* batches:{&casters,&visible}) for(auto& [key,batch]:*batches) { (void)key; batch.offset=UINT(data.size()); data.insert(data.end(),batch.data.begin(),batch.data.end()); }
    if(data.size()>UINT_MAX/sizeof(Instance)) throw std::runtime_error("Too many rendering instances");
    if(!data.empty()) {
        if(data.size()>p.instanceCapacity) {
            p.instances.Reset(); p.instanceCapacity=UINT(data.size());
            D3D11_BUFFER_DESC desc{}; desc.ByteWidth=p.instanceCapacity*sizeof(Instance); desc.Usage=D3D11_USAGE_DYNAMIC;
            desc.BindFlags=D3D11_BIND_VERTEX_BUFFER; desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            check(p.device->CreateBuffer(&desc,nullptr,&p.instances),"instance buffer");
        }
        D3D11_MAPPED_SUBRESOURCE mapped{}; check(p.context->Map(p.instances.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"map instances");
        std::memcpy(mapped.pData,data.data(),data.size()*sizeof(Instance)); p.context->Unmap(p.instances.Get(),0);
    }
    ID3D11ShaderResourceView* empty[2]={nullptr,nullptr}; p.context->PSSetShaderResources(0,2,empty);
    p.context->OMSetBlendState(nullptr,nullptr,0xffffffff);
    p.context->IASetInputLayout(p.layout.Get()); p.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    p.context->VSSetShader(p.vertexShader.Get(),nullptr,0);
    p.context->VSSetConstantBuffers(0,1,p.constants.GetAddressOf()); p.context->PSSetConstantBuffers(0,1,p.constants.GetAddressOf());
    auto draw=[&](auto& batches,bool shadow) {
        for(auto& [key,batch]:batches) {
            (void)key;
            p.context->RSSetState(shadow ? p.shadowRaster.Get() : batch.twoSided ? (batch.mirrored ? p.rasterDoubleMirrored.Get() : p.raster.Get()) : (batch.mirrored ? p.rasterMirrored.Get() : p.rasterBack.Get()));
            ID3D11Buffer* buffers[]={batch.mesh->vertices.Get(),p.instances.Get()}; UINT strides[]={sizeof(Vertex),sizeof(Instance)},offsets[]={0,0};
            p.context->IASetVertexBuffers(0,2,buffers,strides,offsets); p.context->IASetIndexBuffer(batch.mesh->indices.Get(),DXGI_FORMAT_R32_UINT,0);
            p.context->PSSetShaderResources(0,1,batch.texture->view.GetAddressOf()); p.context->PSSetSamplers(0,1,batch.texture->sampler.GetAddressOf());
            p.context->DrawIndexedInstanced(batch.mesh->count,UINT(batch.data.size()),0,0,batch.offset);
            if(shadow) ++p.stats.shadowDraws;
            else { ++p.stats.draws; p.stats.instances+=batch.data.size(); p.stats.triangles+=batch.mesh->count/3*batch.data.size(); }
        }
    };
    if(frame.effects.y!=0) {
        p.context->OMSetRenderTargets(0,nullptr,p.shadowDepth.Get()); p.context->ClearDepthStencilView(p.shadowDepth.Get(),D3D11_CLEAR_DEPTH,1,0);
        D3D11_VIEWPORT shadowViewport{0,0,2048,2048,0,1}; p.context->RSSetViewports(1,&shadowViewport);
        XMStoreFloat4x4(&frame.viewProjection,XMMatrixTranspose(lightVP)); p.context->UpdateSubresource(p.constants.Get(),0,nullptr,&frame,0,0);
        p.context->PSSetShader(p.shadowPixel.Get(),nullptr,0); draw(casters,true);
    }
    const float clear[]={environment.enabled ? environment.sky.r : .025f,environment.enabled ? environment.sky.g : .035f,environment.enabled ? environment.sky.b : .06f,1};
    p.context->OMSetRenderTargets(1,p.color.GetAddressOf(),p.depth.Get()); p.context->ClearRenderTargetView(p.color.Get(),clear); p.context->ClearDepthStencilView(p.depth.Get(),D3D11_CLEAR_DEPTH,1,0);
    D3D11_VIEWPORT viewport{0,0,float(p.renderWidth),float(p.renderHeight),0,1}; p.context->RSSetViewports(1,&viewport);
    XMStoreFloat4x4(&frame.viewProjection,XMMatrixTranspose(view*projection)); p.context->UpdateSubresource(p.constants.Get(),0,nullptr,&frame,0,0);
    p.context->PSSetShader(p.pixelShader.Get(),nullptr,0);
    if(frame.effects.y!=0) { p.context->PSSetShaderResources(1,1,p.shadowView.GetAddressOf()); p.context->PSSetSamplers(1,1,p.shadowSampler.GetAddressOf()); }
    draw(visible,false);
    if(overlay) {
        p.context->OMSetRenderTargets(1,p.color.GetAddressOf(),nullptr);
        ComPtr<ID3D11Resource> resource; overlay->view->GetResource(&resource);
        ComPtr<ID3D11Texture2D> texture; check(resource.As(&texture),"overlay texture"); D3D11_TEXTURE2D_DESC description{}; texture->GetDesc(&description);
        const float scale=std::min({p.canvasOverlay ? 100.0f : 1.0f,float(p.renderWidth)/float(description.Width),float(p.renderHeight)/float(description.Height)});
        D3D11_VIEWPORT panel{p.canvasOverlay ? (p.renderWidth-description.Width*scale)*.5f : 0,p.canvasOverlay ? (p.renderHeight-description.Height*scale)*.5f : 0,description.Width*scale,description.Height*scale,0,1};
        p.context->RSSetViewports(1,&panel); p.context->RSSetState(p.raster.Get()); p.context->OMSetBlendState(p.overlayBlend.Get(),nullptr,0xffffffff);
        p.context->IASetInputLayout(nullptr); p.context->VSSetShader(p.screenVertex.Get(),nullptr,0); p.context->PSSetShader(p.screenPixel.Get(),nullptr,0);
        p.context->PSSetShaderResources(0,1,overlay->view.GetAddressOf()); p.context->PSSetSamplers(0,1,p.screenSampler.GetAddressOf()); p.context->Draw(3,0);
    }
    p.context->OMSetBlendState(nullptr,nullptr,0xffffffff); p.context->OMSetRenderTargets(1,p.back.GetAddressOf(),nullptr); p.context->RSSetState(p.raster.Get());
    viewport.Width=float(p.width); viewport.Height=float(p.height); p.context->RSSetViewports(1,&viewport);
    p.context->IASetInputLayout(nullptr); p.context->VSSetShader(p.screenVertex.Get(),nullptr,0); p.context->PSSetShader(p.screenPixel.Get(),nullptr,0);
    p.context->PSSetShaderResources(0,1,p.targetView.GetAddressOf()); p.context->PSSetSamplers(0,1,p.screenSampler.GetAddressOf()); p.context->Draw(3,0);
    p.context->PSSetShaderResources(0,2,empty);
    p.stats.cpuMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
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
