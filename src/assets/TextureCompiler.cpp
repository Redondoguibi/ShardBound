#include "assets/TextureCompiler.h"
#include "assets/Fingerprint.h"
#include "core/Diagnostic.h"
#include <DirectXTex.h>
#include <windows.h>
#include <cstring>
namespace game {
namespace {
struct ComScope {
    HRESULT result = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComScope() { if (FAILED(result) && result != RPC_E_CHANGED_MODE) throw std::runtime_error("COM initialization failed"); }
    ~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
};
void check(HRESULT result, const std::filesystem::path& source) {
    if (FAILED(result)) throw DiagnosticError({"ASSET201","Texture import/cache failed",source.string()});
}
TextureData pixels(const DirectX::ScratchImage& image) {
    const auto* input = image.GetImage(0,0,0);
    if (!input || input->width > 16384 || input->height > 16384) throw DiagnosticError({"ASSET202","Invalid texture dimensions"});
    DirectX::ScratchImage converted;
    if (input->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        check(DirectX::Convert(*input,DXGI_FORMAT_R8G8B8A8_UNORM,DirectX::TEX_FILTER_DEFAULT,0.5f,converted),{});
        input = converted.GetImage(0,0,0);
    }
    TextureData output{static_cast<unsigned>(input->width),static_cast<unsigned>(input->height)};
    output.pixels.resize(input->width*input->height*4);
    for (size_t y=0;y<input->height;++y) std::memcpy(output.pixels.data()+y*input->width*4,input->pixels+y*input->rowPitch,input->width*4);
    return output;
}
}
TextureData decodeTexture(const std::uint8_t* bytes, size_t size) {
    ComScope com;
    DirectX::ScratchImage image;
    check(DirectX::LoadFromWICMemory(bytes,size,DirectX::WIC_FLAGS_FORCE_RGB,nullptr,image),{});
    return pixels(image);
}
TextureData compileTexture(const std::filesystem::path& source, const std::filesystem::path& cache, bool srgb, bool nearest) {
    ComScope com;
    const auto key = fingerprint(source,std::string("texture-v1|")+(srgb?"srgb":"linear")+(nearest?"nearest":"linear"));
    const auto target = cache / (key + ".dds");
    DirectX::ScratchImage image;
    if (std::filesystem::is_regular_file(target) && SUCCEEDED(DirectX::LoadFromDDSFile(target.c_str(),DirectX::DDS_FLAGS_NONE,nullptr,image))) {
        try { auto output = pixels(image); output.cacheHit = true; return output; } catch (const DiagnosticError&) { image.Release(); }
    }
    check(DirectX::LoadFromWICFile(source.c_str(),DirectX::WIC_FLAGS_FORCE_RGB,nullptr,image),source);
    auto output = pixels(image);
    std::filesystem::create_directories(cache);
    const auto temp = cache / (key + ".tmp.dds");
    check(DirectX::SaveToDDSFile(image.GetImages(),image.GetImageCount(),image.GetMetadata(),DirectX::DDS_FLAGS_NONE,temp.c_str()),source);
    if (!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw DiagnosticError({"ASSET203","Cannot publish compiled cache",target.string()});
    return output;
}
}
