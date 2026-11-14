#include "ecs/RenderSystem.h"
namespace game {
std::vector<RenderItem> collectRenderItems(World& world) {
    std::vector<RenderItem> items;
    for (const auto id:world.entities()) {
        if (world.has<ActiveComponent>(id) && !world.get<ActiveComponent>(id).value) continue;
        if (!world.has<ModelComponent>(id) || !world.has<TransformComponent>(id)) continue;
        const auto& visual=world.get<ModelComponent>(id);
        const auto& model=visual.model;
        if (!model) continue;
        const auto transform=world.worldMatrix(id);
        for (size_t i=0;i<model->parts.size();++i) {
            auto item=model->parts[i];
            if(visual.shader && visual.shader->shader) item.material.shader=visual.shader;
            if(i<model->partNodes.size() && model->partNodes[i]<visual.nodeWorld.size()) item.world=visual.nodeWorld[model->partNodes[i]];
            if(visual.nodeWorld.empty() && i<model->names.size()) for(const auto& [prefix,pose]:visual.poses)
                if(model->names[i].starts_with(prefix)) { item.world=multiply(item.world,pose); break; }
            item.material.color.r*=visual.tint.r; item.material.color.g*=visual.tint.g; item.material.color.b*=visual.tint.b;
            item.world=multiply(item.world,transform); items.push_back(std::move(item));
        }
    }
    return items;
}
}
