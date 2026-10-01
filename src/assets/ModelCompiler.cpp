#include "assets/ModelCompiler.h"
#include "assets/Fingerprint.h"
#include "core/Diagnostic.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <fstream>
#include <functional>
#include <cmath>
#include <numeric>
namespace game {
namespace {
using Json = nlohmann::json;
[[noreturn]] void fail(const std::filesystem::path& file,const std::string& message) { throw DiagnosticError({"ASSET401",message,file.string()}); }
std::span<const std::byte> bytes(const fastgltf::DataSource& source) {
    return std::visit([](const auto& data)->std::span<const std::byte> {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T,fastgltf::sources::Array> || std::is_same_v<T,fastgltf::sources::Vector>) return {data.bytes.data(),data.bytes.size()};
        else if constexpr (std::is_same_v<T,fastgltf::sources::ByteView>) return {data.bytes.data(),data.bytes.size()};
        else throw DiagnosticError({"ASSET402","v0 models require embedded buffers/images (GLB or data URI)"});
    },source);
}
TextureData image(const fastgltf::Asset& asset,size_t index) {
    const auto& source = asset.images.at(index).data;
    std::span<const std::byte> data;
    if (auto view = std::get_if<fastgltf::sources::BufferView>(&source)) {
        const auto& bv = asset.bufferViews.at(view->bufferViewIndex);
        auto buffer = bytes(asset.buffers.at(bv.bufferIndex).data);
        if (bv.byteOffset > buffer.size() || bv.byteLength > buffer.size()-bv.byteOffset) throw DiagnosticError({"ASSET403","Image buffer view out of bounds"});
        data = buffer.subspan(bv.byteOffset,bv.byteLength);
    } else data = bytes(source);
    return decodeTexture(reinterpret_cast<const std::uint8_t*>(data.data()),data.size());
}
void validatePart(const ModelPartData& p) {
    if ((p.alphaMode!=AlphaMode::Opaque && p.alphaMode!=AlphaMode::Mask) || !std::isfinite(p.alphaCutoff) || p.alphaCutoff<0)
        throw DiagnosticError({"ASSET404","Invalid model alpha mode/cutoff"});
    if (p.vertices.empty() || p.vertices.size() > 10000000 || p.indices.empty() || p.indices.size()%3 || p.indices.size()>30000000)
        throw DiagnosticError({"ASSET404","Invalid model geometry size"});
    for (auto i : p.indices) if (i>=p.vertices.size()) throw DiagnosticError({"ASSET404","Index outside model vertices"});
    if (!p.texture.pixels.empty() && (!p.texture.width || !p.texture.height || p.texture.width>16384 || p.texture.height>16384 || p.texture.pixels.size()!=size_t(p.texture.width)*p.texture.height*4))
        throw DiagnosticError({"ASSET404","Invalid cached image dimensions"});
}
Json encode(const ModelData& model) {
    Json result = Json::array();
    for (const auto& part : model.parts) {
        std::vector<float> vertices;
        for (const auto& v:part.vertices) vertices.insert(vertices.end(),{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.uv.x,v.uv.y});
        result.push_back({{"name",part.name},{"vertices",vertices},{"indices",part.indices},{"transform",part.transform.values},
            {"color",{part.color.r,part.color.g,part.color.b,part.color.a}},{"width",part.texture.width},{"height",part.texture.height},{"pixels",Json::binary(part.texture.pixels)},
            {"alphaMode",static_cast<int>(part.alphaMode)},{"alphaCutoff",part.alphaCutoff},{"doubleSided",part.doubleSided}});
    }
    return result;
}
ModelData decode(const Json& data) {
    if (!data.is_array() || data.empty() || data.size()>10000) throw std::runtime_error("Invalid model cache");
    ModelData result;
    for (const auto& p:data) {
        ModelPartData part; part.name=p.at("name").get<std::string>();
        auto vertices=p.at("vertices").get<std::vector<float>>();
        if (vertices.size()%8) throw std::runtime_error("Invalid vertex cache");
        for (size_t i=0;i<vertices.size();i+=8) part.vertices.push_back({{vertices[i],vertices[i+1],vertices[i+2]},{vertices[i+3],vertices[i+4],vertices[i+5]},{vertices[i+6],vertices[i+7]}});
        part.indices=p.at("indices").get<std::vector<std::uint32_t>>(); part.transform.values=p.at("transform").get<std::array<float,16>>();
        auto color=p.at("color").get<std::array<float,4>>(); part.color={color[0],color[1],color[2],color[3]};
        part.texture.width=p.at("width"); part.texture.height=p.at("height"); part.texture.pixels=p.at("pixels").get_binary();
        part.alphaMode=static_cast<AlphaMode>(p.at("alphaMode").get<int>());
        part.alphaCutoff=p.at("alphaCutoff"); part.doubleSided=p.at("doubleSided");
        validatePart(part); result.parts.push_back(std::move(part));
    }
    result.cacheHit=true; return result;
}
}
ModelData compileModel(const std::filesystem::path& source,const std::filesystem::path& cache) {
    const auto key=fingerprint(source,"static-model-v2-fastgltf-0.9-mask-double-sided");
    const auto target=cache/(key+".model");
    if (std::filesystem::is_regular_file(target) && std::filesystem::file_size(target)<256*1024*1024) {
        try { std::ifstream input(target,std::ios::binary); return decode(Json::from_cbor(input)); } catch (const std::exception&) {}
    }
    if (std::filesystem::file_size(source)>256*1024*1024) fail(source,"Model exceeds v0 256 MiB import limit");
    auto file=fastgltf::MappedGltfFile::FromPath(source);
    if (!file) fail(source,std::string(fastgltf::getErrorMessage(file.error())));
    fastgltf::Parser parser;
    auto loaded=parser.loadGltf(file.get(),source.parent_path(),fastgltf::Options::GenerateMeshIndices);
    if (!loaded) fail(source,std::string(fastgltf::getErrorMessage(loaded.error())));
    auto& asset=loaded.get();
    if (fastgltf::validate(asset)!=fastgltf::Error::None) fail(source,"Invalid GLTF asset");
    if (!asset.skins.empty() || !asset.animations.empty()) fail(source,"Skinning/animation is not implemented in the static model v0 importer");
    for (const auto& buffer:asset.buffers) (void)bytes(buffer.data);
    if (asset.scenes.empty()) fail(source,"Model has no scene");
    std::vector<bool> visiting(asset.nodes.size());
    std::function<void(size_t,size_t)> validateNode=[&](size_t index,size_t depth) {
        if (index>=asset.nodes.size() || depth>64 || visiting[index]) fail(source,"Invalid or cyclic node hierarchy");
        visiting[index]=true;
        for (auto child:asset.nodes[index].children) validateNode(child,depth+1);
        visiting[index]=false;
    };
    const auto scene=asset.defaultScene.value_or(0);
    for (auto node:asset.scenes.at(scene).nodeIndices) validateNode(node,0);
    ModelData output;
    fastgltf::iterateSceneNodes(asset,scene,fastgltf::math::fmat4x4(),[&](fastgltf::Node& node,fastgltf::math::fmat4x4 matrix) {
        if (!node.meshIndex) return;
        const auto& mesh=asset.meshes.at(*node.meshIndex);
        for (const auto& primitive:mesh.primitives) {
            if (primitive.type!=fastgltf::PrimitiveType::Triangles || !primitive.indicesAccessor) fail(source,"Only triangle primitives supported in v0");
            ModelPartData part; part.name=std::string(node.name)+"/"+std::to_string(output.parts.size());
            for (size_t r=0;r<4;++r) for (size_t c=0;c<4;++c) part.transform.values[r*4+c]=matrix[r][c];
            auto position=primitive.findAttribute("POSITION"), normal=primitive.findAttribute("NORMAL"), uv=primitive.findAttribute("TEXCOORD_0");
            if (position==primitive.attributes.end() || normal==primitive.attributes.end()) fail(source,"Static model v0 requires POSITION and NORMAL");
            const auto& positions=asset.accessors.at(position->accessorIndex);
            if (positions.count>10000000) fail(source,"Vertex limit exceeded");
            part.vertices.resize(positions.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,positions,[&](auto v,size_t i) { part.vertices.at(i).position={v.x(),v.y(),v.z()}; });
            if (asset.accessors.at(normal->accessorIndex).count!=positions.count) fail(source,"Normal count differs from POSITION");
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,asset.accessors.at(normal->accessorIndex),[&](auto v,size_t i) { part.vertices.at(i).normal={v.x(),v.y(),v.z()}; });
            if (uv!=primitive.attributes.end()) {
                if (asset.accessors.at(uv->accessorIndex).count!=positions.count) fail(source,"UV count differs from POSITION");
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset,asset.accessors.at(uv->accessorIndex),[&](auto v,size_t i) { part.vertices.at(i).uv={v.x(),v.y()}; });
            }
            const auto& indices=asset.accessors.at(*primitive.indicesAccessor);
            if (indices.count>30000000) fail(source,"Index limit exceeded");
            fastgltf::iterateAccessor<std::uint32_t>(asset,indices,[&](auto i) { part.indices.push_back(i); });
            if (primitive.materialIndex) {
                const auto& material=asset.materials.at(*primitive.materialIndex);
                if (material.alphaMode==fastgltf::AlphaMode::Blend) fail(source,"Alpha BLEND materials are not yet supported; use OPAQUE or MASK");
                part.alphaMode=material.alphaMode==fastgltf::AlphaMode::Mask ? AlphaMode::Mask : AlphaMode::Opaque;
                part.alphaCutoff=material.alphaCutoff; part.doubleSided=material.doubleSided;
                auto c=material.pbrData.baseColorFactor; part.color={c[0],c[1],c[2],c[3]};
                if (material.pbrData.baseColorTexture) {
                    const auto& texture=asset.textures.at(material.pbrData.baseColorTexture->textureIndex);
                    if (!texture.imageIndex || material.pbrData.baseColorTexture->texCoordIndex!=0 || uv==primitive.attributes.end()) fail(source,"Base texture requires image and TEXCOORD_0");
                    part.texture=image(asset,*texture.imageIndex);
                }
            }
            validatePart(part); output.parts.push_back(std::move(part));
        }
    });
    if (output.parts.empty()) fail(source,"Model has no drawable mesh");
    std::filesystem::create_directories(cache);
    const auto temporary=cache/(key+".tmp.model");
    const auto compiled=Json::to_cbor(encode(output));
    { std::ofstream stream(temporary,std::ios::binary); stream.write(reinterpret_cast<const char*>(compiled.data()),static_cast<std::streamsize>(compiled.size())); if (!stream) fail(source,"Cannot write model cache"); }
    if (!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) fail(source,"Cannot publish model cache");
    return output;
}
}
