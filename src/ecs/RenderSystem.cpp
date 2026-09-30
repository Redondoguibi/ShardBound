#include "ecs/RenderSystem.h"
namespace game {
std::vector<RenderItem> collectRenderItems(World& world) {
    std::vector<RenderItem> items;
    for (const auto id:world.entities()) {
        if (world.has<ActiveComponent>(id) && !world.get<ActiveComponent>(id).value) continue;
        if (!world.has<ModelComponent>(id) || !world.has<TransformComponent>(id)) continue;
        const auto& model=world.get<ModelComponent>(id).model;
        if (!model) continue;
        const auto transform=world.worldMatrix(id);
        for (auto item:model->parts) { item.world=multiply(item.world,transform); items.push_back(std::move(item)); }
    }
    return items;
}
}
