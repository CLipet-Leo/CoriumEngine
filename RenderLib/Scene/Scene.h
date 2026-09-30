#pragma once

// Stockage des composants pour un type donné
template<typename T>
using ComponentMap = std::unordered_map<EntityID, T>;

class CORIUM_API Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    // --- Gestion des entités ---
    Entity  CreateEntity(const std::string& name = "Entity");
    void    DestroyEntity(EntityID id);
    Entity* GetEntity(EntityID id);
    bool    IsValid(EntityID id) const;

    // --- Gestion des composants ---
    template<typename T>
    T& AddComponent(EntityID id, T component = {});

    template<typename T>
    T* GetComponent(EntityID id);

    template<typename T>
    bool HasComponent(EntityID id) const;

    template<typename T>
    void RemoveComponent(EntityID id);

    // --- Hiérarchie ---
    void SetParent(EntityID child, EntityID parent);
    void RemoveParent(EntityID child);
    const std::vector<EntityID>& GetRootEntities() const { return m_roots; }

    // --- Accès aux registres (pour les systèmes) ---
    ComponentMap<TransformComponent>& Transforms() { return m_transforms; }
    ComponentMap<HierarchyComponent>& Hierarchies() { return m_hierarchies; }
    ComponentMap<MeshComponent>& Meshes() { return m_meshes; }
    ComponentMap<CameraComponent>& Cameras() { return m_cameras; }
    ComponentMap<LightComponent>& Lights() { return m_lights; }

private:
    EntityID                              m_nextId = 0;
    std::unordered_map<EntityID, Entity>  m_entities;
    std::vector<EntityID>                 m_roots;    // entités sans parent

    ComponentMap<TransformComponent>  m_transforms;
    ComponentMap<HierarchyComponent>  m_hierarchies;
    ComponentMap<MeshComponent>       m_meshes;
    ComponentMap<CameraComponent>     m_cameras;
    ComponentMap<LightComponent>      m_lights;

    // Sélectionne le bon registre selon le type (spécialisé en .cpp)
    template<typename T> ComponentMap<T>& GetMap();
};

// Déclarations des spécialisations (définies dans Scene.cpp)
template<> ComponentMap<TransformComponent>& Scene::GetMap<TransformComponent>();
template<> ComponentMap<HierarchyComponent>& Scene::GetMap<HierarchyComponent>();
template<> ComponentMap<MeshComponent>& Scene::GetMap<MeshComponent>();
template<> ComponentMap<CameraComponent>& Scene::GetMap<CameraComponent>();
template<> ComponentMap<LightComponent>& Scene::GetMap<LightComponent>();

// --- Implémentation template (inline) ---

template<typename T>
T& Scene::AddComponent(EntityID id, T component)
{
    CE_ASSERT(IsValid(id));
    auto& map = GetMap<T>();
    map[id] = std::move(component);
    return map[id];
}

template<typename T>
T* Scene::GetComponent(EntityID id)
{
    auto& map = GetMap<T>();
    auto  it = map.find(id);
    return (it != map.end()) ? &it->second : nullptr;
}

template<typename T>
bool Scene::HasComponent(EntityID id) const
{
    return const_cast<Scene*>(this)->GetMap<T>().contains(id);
}

template<typename T>
void Scene::RemoveComponent(EntityID id)
{
    GetMap<T>().erase(id);
}