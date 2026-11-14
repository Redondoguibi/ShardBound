#include "assets/ShaderCompiler.h"
#include "assets/AssetId.h"
#include "core/Diagnostic.h"
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include <fstream>
#include <map>
#include <regex>
namespace game {
ShaderSource shaderSource(const std::string& id,const nlohmann::json& entry) {
    (void)AssetId(id);
    if(!entry.is_object()) throw std::runtime_error("Shader entry must be an object");
    for(const auto& [key,value]:entry.items()) { (void)value;if(key!="source" && key!="kind" && key!="version") throw std::runtime_error("Unknown shader field: "+key); }
    if(!entry.contains("version") || entry.at("version")!=1) throw std::runtime_error("Shader ABI version must be 1");
    const auto kind=entry.at("kind").get<std::string>();
    if(kind!="surface" && kind!="postprocess") throw std::runtime_error("Shader kind must be surface or postprocess");
    return {id,entry.at("source").get<std::string>(),kind=="surface"?ShaderKind::Surface:ShaderKind::PostProcess};
}
std::string readShaderText(const std::filesystem::path& file) {
    const auto size=std::filesystem::file_size(file);
    if(size==0 || size>256*1024) throw std::runtime_error("Shader source must contain 1..262144 bytes: "+file.string());
    std::ifstream input(file,std::ios::binary);std::string text(size,'\0');
    if(!input.read(text.data(),static_cast<std::streamsize>(size))) throw std::runtime_error("Cannot read shader: "+file.string());
    if(text.find('\0')!=std::string::npos) throw std::runtime_error("NUL in shader source: "+file.string());
    return text;
}
CompiledShader compileShader(const VirtualFileSystem& vfs,const ShaderSource& source,const std::string& library) {
    const AssetId id(source.id);
    struct Includes final : ID3DInclude {
        struct File { std::string text;std::filesystem::path relative;unsigned depth; };
        const VirtualFileSystem& vfs;std::string ns;std::filesystem::path root;
        std::map<const void*,std::unique_ptr<File>> open;
        unsigned count=0;size_t bytes=0;std::string error;
        Includes(const VirtualFileSystem& v,const ShaderSource& s):vfs(v),ns(AssetId(s.id).nameSpace()),root(s.source) {}
        HRESULT __stdcall Open(D3D_INCLUDE_TYPE type,LPCSTR name,LPCVOID parent,LPCVOID* data,UINT* size) override {
            try {
                if(type!=D3D_INCLUDE_LOCAL) throw std::runtime_error("Only quoted local shader includes are supported");
                const auto found=open.find(parent);const unsigned depth=found==open.end()?0:found->second->depth+1;
                if(++count>64 || depth>16) throw std::runtime_error("Shader include limit exceeded (64 files / 16 levels)");
                const auto relative=found==open.end()?root:found->second->relative.parent_path()/std::filesystem::path(name);
                if(found==open.end() && std::string_view(name)!="mod.hlsl") throw std::runtime_error("Unknown engine shader include");
                const auto path=vfs.resolve(ns,relative);
                auto file=std::make_unique<File>(File{readShaderText(path),relative,depth});
                bytes+=file->text.size();if(bytes>2*1024*1024) throw std::runtime_error("Shader include bytes exceed 2 MiB");
                file->text="#line 1 \""+path.generic_string()+"\"\n"+file->text;
                *data=file->text.data();*size=UINT(file->text.size());open.emplace(*data,std::move(file));return S_OK;
            } catch(const std::exception& e) {error=e.what();return E_FAIL;}
        }
        HRESULT __stdcall Close(LPCVOID data) override {open.erase(data);return S_OK;}
    };
    auto compile=[&](const char* entry) {
        Includes includes(vfs,source);Microsoft::WRL::ComPtr<ID3DBlob> code,errors;
        const D3D_SHADER_MACRO macros[]={{source.kind==ShaderKind::Surface?"SHARDBOUND_SURFACE":"SHARDBOUND_POST","1"},{nullptr,nullptr}};
        const auto result=D3DCompile(library.data(),library.size(),"shardbound-shader-abi-1.hlsl",macros,&includes,entry,"ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&code,&errors);
        if(FAILED(result)) {
            std::string message=errors?std::string(static_cast<const char*>(errors->GetBufferPointer())):"HLSL compilation failed";
            if(!includes.error.empty()) message+="\n"+includes.error;
            Diagnostic d{"SHADER101",message,source.source.generic_string(),id.nameSpace(),source.id};
            std::smatch match;if(std::regex_search(message,match,std::regex(R"(([^\r\n]+)\((\d+),(\d+)(?:-\d+)?\):)"))) {d.file=match[1];d.line=unsigned(std::stoul(match[2]));d.column=unsigned(std::stoul(match[3]));}
            if(d.file=="mod.hlsl") d.file=source.source.generic_string();
            throw DiagnosticError(std::move(d));
        }
        // Only the documented bindings may be consumed. Prevent undeclared resources from silently rendering black.
        Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
        if(FAILED(D3DReflect(code->GetBufferPointer(),code->GetBufferSize(),IID_PPV_ARGS(&reflection)))) throw std::runtime_error("Cannot reflect shader");
        D3D11_SHADER_DESC desc{};reflection->GetDesc(&desc);
        for(UINT i=0;i<desc.BoundResources;++i) {
            D3D11_SHADER_INPUT_BIND_DESC binding{};reflection->GetResourceBindingDesc(i,&binding);
            const std::string name=binding.Name;
            const bool valid=binding.BindCount==1 && (
                (binding.Type==D3D_SIT_CBUFFER && ((name=="Frame" && binding.BindPoint==0)||(name=="ModParameters" && binding.BindPoint==1))) ||
                (binding.Type==D3D_SIT_TEXTURE && ((name=="baseTexture" && binding.BindPoint==0)||(name=="shadowMap" && binding.BindPoint==1 && source.kind==ShaderKind::Surface)||(name=="modTexture" && binding.BindPoint==2))) ||
                (binding.Type==D3D_SIT_SAMPLER && ((name=="baseSampler" && binding.BindPoint==0)||(name=="shadowSampler" && binding.BindPoint==1 && source.kind==ShaderKind::Surface)||(name=="modSampler" && binding.BindPoint==2))));
            if(!valid) throw DiagnosticError({"SHADER102","Unsupported shader binding: "+name,source.source.string(),id.nameSpace(),source.id});
        }
        const auto* first=static_cast<const std::uint8_t*>(code->GetBufferPointer());return std::vector<std::uint8_t>(first,first+code->GetBufferSize());
    };
    CompiledShader result;result.kind=source.kind;
    result.pixel=compile(source.kind==ShaderKind::Surface?"pixelMain":"modPostMain");
    if(source.kind==ShaderKind::Surface) result.shadow=compile("shadowPixel");
    return result;
}
}
