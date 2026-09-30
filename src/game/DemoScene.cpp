#include "game/DemoScene.h"
namespace game {
MeshHandle createDemoCube(Renderer& renderer) {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    auto face = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n) {
        const auto offset = static_cast<std::uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {{a,n,{0,1}}, {b,n,{0,0}}, {c,n,{1,0}}, {d,n,{1,1}}});
        for (auto i : {0u,1u,2u,0u,2u,3u}) indices.push_back(offset+i);
    };
    face({-1,-1,-1},{-1,1,-1},{1,1,-1},{1,-1,-1},{0,0,-1});
    face({1,-1,1},{1,1,1},{-1,1,1},{-1,-1,1},{0,0,1});
    face({-1,-1,1},{-1,1,1},{-1,1,-1},{-1,-1,-1},{-1,0,0});
    face({1,-1,-1},{1,1,-1},{1,1,1},{1,-1,1},{1,0,0});
    face({-1,1,-1},{-1,1,1},{1,1,1},{1,1,-1},{0,1,0});
    face({-1,-1,1},{-1,-1,-1},{1,-1,-1},{1,-1,1},{0,-1,0});
    return renderer.createMesh(vertices,indices);
}
}
