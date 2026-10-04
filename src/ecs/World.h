#pragma once
#include "assets/AssetManager.h"
#include <variant>
#include "core/Data.h"
namespace game {
struct EntityId { std::uint64_t value = 0; auto operator<=>(const EntityId&) const = default; explicit operator bool() const { return value!=0; } };
struct TransformComponent { Transform local; };
struct NameComponent { std::string name; };
struct ModelComponent { AssetId definition; ModelHandle model; Color tint{}; std::map<std::string,Matrix> poses; };
struct ActiveComponent { bool value = true; };
struct HealthComponent { float current = 100, maximum = 100; };
struct InventoryComponent { std::map<std::string,std::uint32_t> items; };
struct InteractableComponent { bool enabled = true; };
struct IdentityComponent { std::string persistent,definition,owner="core"; };
struct CustomComponent { unsigned version=1; Data data; };
using Component = std::variant<TransformComponent,NameComponent,ModelComponent,ActiveComponent,HealthComponent,InventoryComponent,InteractableComponent,IdentityComponent>;
template<class T> constexpr size_t componentIndex() {
    if constexpr (std::is_same_v<T,TransformComponent>) return 0;
    else if constexpr (std::is_same_v<T,NameComponent>) return 1;
    else if constexpr (std::is_same_v<T,ModelComponent>) return 2;
    else if constexpr (std::is_same_v<T,ActiveComponent>) return 3;
    else if constexpr (std::is_same_v<T,HealthComponent>) return 4;
    else if constexpr (std::is_same_v<T,InventoryComponent>) return 5;
    else if constexpr (std::is_same_v<T,InteractableComponent>) return 6;
    else if constexpr (std::is_same_v<T,IdentityComponent>) return 7;
    else static_assert(sizeof(T)==0,"Unregistered component type");
}
class World {
public:
    World();
    ~World();
    EntityId create(std::string name = {});
    bool alive(EntityId id) const;
    void destroy(EntityId id);
    EntityId find(std::string_view name) const;
    EntityId findPersistent(std::string_view key) const;
    std::vector<EntityId> entities() const;
    template<class T> void add(EntityId id,T value) { put(id,Component(std::move(value))); }
    template<class T> T& get(EntityId id) { return *static_cast<T*>(component(id,componentIndex<T>(),true)); }
    template<class T> bool has(EntityId id) { return component(id,componentIndex<T>(),false)!=nullptr; }
    template<class T> void remove(EntityId id) { removeComponent(id,componentIndex<T>()); }
    void setParent(EntityId child,EntityId parent);
    EntityId parent(EntityId child) const;
    Matrix worldMatrix(EntityId id);
    void setCustom(EntityId entity,const AssetId& type,CustomComponent value);
    const std::map<std::string,CustomComponent>& custom(EntityId entity) const;
    bool removeCustom(EntityId entity,const AssetId& type);
    void swapState(World& other) noexcept;
private:
    void put(EntityId id,Component value);
    void* component(EntityId id,size_t kind,bool required);
    void removeComponent(EntityId id,size_t kind);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
