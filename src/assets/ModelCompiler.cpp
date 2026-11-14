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
#include <set>
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
        result.push_back({{"node",part.node},{"name",part.name},{"vertices",vertices},{"indices",part.indices},{"transform",part.transform.values},
            {"color",{part.color.r,part.color.g,part.color.b,part.color.a}},{"width",part.texture.width},{"height",part.texture.height},{"pixels",Json::binary(part.texture.pixels)},
            {"alphaMode",static_cast<int>(part.alphaMode)},{"alphaCutoff",part.alphaCutoff},{"doubleSided",part.doubleSided}});
    }
    Json nodes=Json::array(),clips=Json::array();
    for(const auto& n:model.hierarchy.nodes) nodes.push_back({{"name",n.name},{"uuid",n.uuid},{"role",n.role},{"parent",n.parent},{"local",n.local.values}});
    for(const auto& c:model.hierarchy.clips) { Json tracks=Json::array(); for(const auto& t:c.tracks) tracks.push_back({{"node",t.node},{"path",t.path},{"step",t.step},{"times",t.times},{"values",t.values}}); clips.push_back({{"name",c.name},{"length",c.length},{"tracks",tracks}}); }
    return {{"parts",result},{"nodes",nodes},{"clips",clips}};
}
ModelData decode(const Json& data) {
    if (!data.is_object() || !data.at("parts").is_array() || data.at("parts").size()>10000) throw std::runtime_error("Invalid model cache");
    ModelData result;
    for (const auto& p:data.at("parts")) {
        ModelPartData part; part.node=p.at("node"); part.name=p.at("name").get<std::string>();
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
    for(const auto& n:data.at("nodes")) { ModelNode node; node.name=n.at("name"); node.uuid=n.at("uuid"); node.role=n.at("role"); node.parent=n.at("parent"); node.local.values=n.at("local").get<std::array<float,16>>(); result.hierarchy.nodes.push_back(node); }
    for(const auto& c:data.at("clips")) { ModelClip clip; clip.name=c.at("name"); clip.length=c.at("length"); for(const auto& t:c.at("tracks")) { ModelTrack track; track.node=t.at("node"); track.path=t.at("path"); track.step=t.at("step"); track.times=t.at("times").get<std::vector<float>>(); track.values=t.at("values").get<std::vector<std::array<float,4>>>(); if(track.node>=result.hierarchy.nodes.size() || track.times.empty() || track.times.size()!=track.values.size()) throw std::runtime_error("Invalid cached track"); clip.tracks.push_back(std::move(track)); } result.hierarchy.clips.push_back(std::move(clip)); }
    validateHierarchy(result.hierarchy);(void)sampleModel(result.hierarchy,"",0);
    for(const auto& p:result.parts) if(p.node>=result.hierarchy.nodes.size()) throw std::runtime_error("Invalid cached mesh node");
    result.cacheHit=true; return result;
}
}
ModelData compileModel(const std::filesystem::path& source,const std::filesystem::path& cache) {
    const auto key=fingerprint(source,"rigid-model-v3-hierarchy-animation-studio");
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
    if (!asset.skins.empty()) fail(source,"Weighted skinning is not supported; animate rigid nodes/groups");
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
    if(asset.nodes.size()>10000 || asset.animations.size()>1024) fail(source,"Model node/clip limit exceeded");
    std::ifstream metadataFile(source,std::ios::binary); std::uint32_t header[5]{}; metadataFile.read(reinterpret_cast<char*>(header),sizeof(header)); Json metadata;
    if(header[0]==0x46546c67) { if(header[3]>64*1024*1024 || header[4]!=0x4e4f534a) fail(source,"Invalid GLB metadata"); std::string text(header[3],' '); metadataFile.read(text.data(),header[3]); if(!metadataFile) fail(source,"Truncated GLB metadata"); metadata=Json::parse(text); }
    else { metadataFile.clear(); metadataFile.seekg(0); metadata=Json::parse(metadataFile); }
    for(size_t i=0;i<asset.nodes.size();i++) {
        const auto& node=asset.nodes[i]; ModelNode n; n.name=std::string(node.name); const auto m=fastgltf::getTransformMatrix(node);
        for(size_t r=0;r<4;r++) for(size_t c=0;c<4;c++) { n.local.values[r*4+c]=m[r][c]; if(!std::isfinite(m[r][c])) fail(source,"Non-finite node transform"); }
        const auto& raw=metadata.at("nodes").at(i); if(raw.contains("extras") && raw["extras"].is_object()) { n.uuid=raw["extras"].value("studio_uuid",std::string{}); n.role=raw["extras"].value("studio_role",std::string{}); }
        output.hierarchy.nodes.push_back(n);
    }
    for(size_t i=0;i<asset.nodes.size();i++) for(auto child:asset.nodes[i].children) { auto& parent=output.hierarchy.nodes.at(child).parent; if(parent!=-1) fail(source,"Node has multiple parents"); parent=int(i); }
    size_t samples=0;
    for(const auto& animation:asset.animations) {
        ModelClip clip; clip.name=animation.name.empty() ? "animation_"+std::to_string(output.hierarchy.clips.size()) : std::string(animation.name);
        for(const auto& previous:output.hierarchy.clips) if(previous.name==clip.name) fail(source,"Duplicate animation name");
        std::set<std::pair<size_t,int>> targets;
        for(const auto& channel:animation.channels) {
            if(!channel.nodeIndex || channel.path==fastgltf::AnimationPath::Weights) fail(source,"Only node translation, rotation and scale animation are supported");
            const auto& sampler=animation.samplers.at(channel.samplerIndex);
            if(sampler.interpolation==fastgltf::AnimationInterpolation::CubicSpline) fail(source,"Bake cubic animation curves to LINEAR samples before import");
            ModelTrack track; track.node=*channel.nodeIndex; track.path=int(channel.path); track.step=sampler.interpolation==fastgltf::AnimationInterpolation::Step;
            if(!targets.emplace(track.node,track.path).second) fail(source,"Duplicate animation channel");
            const auto& input=asset.accessors.at(sampler.inputAccessor); const auto& values=asset.accessors.at(sampler.outputAccessor);
            samples+=input.count; if(samples>2000000 || input.count!=values.count || !input.count) fail(source,"Invalid or oversized animation track");
            fastgltf::iterateAccessor<float>(asset,input,[&](float t){if(!std::isfinite(t) || t<0 || (!track.times.empty() && t<=track.times.back())) fail(source,"Invalid animation time"); track.times.push_back(t);});
            if(track.path==2) fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset,values,[&](auto v){track.values.push_back({v[0],v[1],v[2],v[3]});});
            else fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset,values,[&](auto v){track.values.push_back({v[0],v[1],v[2],0});});
            for(const auto& v:track.values) { for(auto x:v) if(!std::isfinite(x)) fail(source,"Non-finite animation value"); if(track.path==2 && v[0]*v[0]+v[1]*v[1]+v[2]*v[2]+v[3]*v[3]<1e-12f) fail(source,"Zero animation quaternion"); }
            clip.length=std::max(clip.length,track.times.back()); clip.tracks.push_back(std::move(track));
        }
        output.hierarchy.clips.push_back(std::move(clip));
    }
    validateHierarchy(output.hierarchy);(void)sampleModel(output.hierarchy,"",0);
    fastgltf::iterateSceneNodes(asset,scene,fastgltf::math::fmat4x4(),[&](fastgltf::Node& node,fastgltf::math::fmat4x4 matrix) {
        if (!node.meshIndex) return;
        const auto& mesh=asset.meshes.at(*node.meshIndex);
        for (const auto& primitive:mesh.primitives) {
            if (primitive.type!=fastgltf::PrimitiveType::Triangles || !primitive.indicesAccessor) fail(source,"Only triangle primitives supported in v0");
            ModelPartData part; part.node=size_t(&node-asset.nodes.data()); part.name=std::string(node.name)+"/"+std::to_string(output.parts.size());
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
    if (output.parts.empty() && std::none_of(output.hierarchy.nodes.begin(),output.hierarchy.nodes.end(),[](const auto& n){return !n.uuid.empty();})) fail(source,"Model has no drawable mesh");
    std::filesystem::create_directories(cache);
    const auto temporary=cache/(key+".tmp.model");
    const auto compiled=Json::to_cbor(encode(output));
    { std::ofstream stream(temporary,std::ios::binary); stream.write(reinterpret_cast<const char*>(compiled.data()),static_cast<std::streamsize>(compiled.size())); if (!stream) fail(source,"Cannot write model cache"); }
    if (!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) fail(source,"Cannot publish model cache");
    return output;
}
}
