#include "pch.h"
#include "Scene.h"

Entity Scene::CreateEntity(const std::string& name)
{
    Entity e;
    e.id = m_nextId++;
    e.name = name;
    m_entities[e.id] = e;

    // Tout nouvel entity a un Transform et est à la racine par défaut
    AddComponent<TransformComponent>(e.id);
    AddComponent<HierarchyComponent>(e.id);
    m_roots.push_back(e.id);

    return e;
}

void Scene::DestroyEntity(EntityID id)
{
    if (!IsValid(id)) return;

    // Détacher les enfants avant de supprimer
    if (auto* h = GetComponent<HierarchyComponent>(id))
    {
        const std::vector<EntityID> children = h->children;
        for (EntityID child : children)
            RemoveParent(child);
        if (h->HasParent())
            RemoveParent(id);
    }

    m_transforms.erase(id);
    m_hierarchies.erase(id);
    m_meshes.erase(id);
    m_cameras.erase(id);
    m_lights.erase(id);

    std::erase(m_roots, id);
    m_entities.erase(id);
}

Entity* Scene::GetEntity(EntityID id)
{
    auto it = m_entities.find(id);
    return (it != m_entities.end()) ? &it->second : nullptr;
}

bool Scene::IsValid(EntityID id) const
{
    return m_entities.contains(id);
}

void Scene::SetParent(EntityID child, EntityID parent)
{
    CE_ASSERT(IsValid(child) && IsValid(parent));
    CE_ASSERT(child != parent);

    // Refuser un cycle : parent ne doit pas être un descendant de child
    for (EntityID p = parent; p != NULL_ENTITY; p = m_hierarchies[p].parent)
    {
        if (p == child)
        {
            CE_ASSERT(false && "SetParent: cycle detected");
            return;
        }
    }

    auto& childH = m_hierarchies[child];
    auto& parentH = m_hierarchies[parent];

    // Retirer de l'ancien parent si nécessaire
    if (childH.HasParent())
        RemoveParent(child);

    childH.parent = parent;
    parentH.children.push_back(child);

    // N'est plus une racine
    std::erase(m_roots, child);

    // Marquer le transform comme dirty
    if (auto* t = GetComponent<TransformComponent>(child))
        t->dirty = true;
}

void Scene::RemoveParent(EntityID child)
{
    auto& childH = m_hierarchies[child];
    if (!childH.HasParent()) return;

    auto& parentH = m_hierarchies[childH.parent];
    std::erase(parentH.children, child);
    childH.parent = NULL_ENTITY;

    // Redevient une racine
    m_roots.push_back(child);

    if (auto* t = GetComponent<TransformComponent>(child))
        t->dirty = true;
}
