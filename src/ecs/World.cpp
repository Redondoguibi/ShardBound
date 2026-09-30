#include "ecs/World.h"
#include "core/Diagnostic.h"
#include <entt/entity/registry.hpp>
#include <atomic>
#include <map>
namespace game {
struct World::Impl {
    entt::registry registry;
    std::map<EntityId,entt::entity> entities;
    std::map<EntityId,EntityId> parents;
    entt::entity require(EntityId id) const {
        const auto it=entities.find(id);
        if (it==entities.end()) throw DiagnosticError({"ECS101","Invalid or destroyed entity #"+std::to_string(id.value)});
        return it->second;
    }
};
World::World() : impl_(std::make_unique<Impl>()) {}
World::~World()=default;
EntityId World::create(std::string name) {
    static std::atomic_uint64_t sequence{1};
    EntityId id{sequence.fetch_add(1)};
    auto raw=impl_->registry.create(); impl_->entities.emplace(id,raw);
    impl_->registry.emplace<NameComponent>(raw,std::move(name));
    impl_->registry.emplace<ActiveComponent>(raw);
    return id;
}
bool World::alive(EntityId id) const { return impl_->entities.contains(id); }
void World::destroy(EntityId id) {
    const auto raw=impl_->require(id);
    impl_->parents.erase(id);
    std::erase_if(impl_->parents,[&](const auto& pair) { return pair.second==id; });
    impl_->registry.destroy(raw); impl_->entities.erase(id);
}
EntityId World::find(std::string_view name) const {
    for (const auto& [id,raw]:impl_->entities) {
        auto component=impl_->registry.try_get<NameComponent>(raw);
        if (component && component->name==name) return id;
    }
    return {};
}
std::vector<EntityId> World::entities() const { std::vector<EntityId> ids; for (const auto& [id,raw]:impl_->entities) { (void)raw; ids.push_back(id); } return ids; }
void World::put(EntityId id,Component value) {
    const auto raw=impl_->require(id);
    std::visit([&](auto&& item) { using T=std::decay_t<decltype(item)>; impl_->registry.emplace_or_replace<T>(raw,std::move(item)); },std::move(value));
}
void* World::component(EntityId id,size_t kind,bool required) {
    auto raw=impl_->require(id); void* result=nullptr;
    switch(kind) {
    case 0: result=impl_->registry.try_get<TransformComponent>(raw); break;
    case 1: result=impl_->registry.try_get<NameComponent>(raw); break;
    case 2: result=impl_->registry.try_get<ModelComponent>(raw); break;
    case 3: result=impl_->registry.try_get<ActiveComponent>(raw); break;
    }
    if (!result && required) throw DiagnosticError({"ECS102","Required component missing on #"+std::to_string(id.value)});
    return result;
}
void World::removeComponent(EntityId id,size_t kind) {
    const auto raw=impl_->require(id);
    switch(kind) {
    case 0: impl_->registry.remove<TransformComponent>(raw); break;
    case 1: impl_->registry.remove<NameComponent>(raw); break;
    case 2: impl_->registry.remove<ModelComponent>(raw); break;
    case 3: impl_->registry.remove<ActiveComponent>(raw); break;
    }
}
void World::setParent(EntityId child,EntityId parentId) {
    impl_->require(child);
    if (!parentId) { impl_->parents.erase(child); return; }
    impl_->require(parentId);
    for (auto ancestor=parentId;ancestor;ancestor=parent(ancestor)) if (ancestor==child) throw DiagnosticError({"ECS103","Transform hierarchy cycle"});
    impl_->parents.insert_or_assign(child,parentId);
}
EntityId World::parent(EntityId id) const {
    impl_->require(id); const auto it=impl_->parents.find(id); return it==impl_->parents.end() ? EntityId{} : it->second;
}
Matrix World::worldMatrix(EntityId id) {
    Matrix result;
    for (auto current=id;current;current=parent(current)) {
        if (has<TransformComponent>(current)) result=multiply(result,transformMatrix(get<TransformComponent>(current).local));
    }
    return result;
}
}
