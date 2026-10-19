#include "pch.h"
#include "TransformSystem.h"

void TransformSystem::Update(Scene& scene)
{
    // Parcours depuis les racines pour respecter l'ordre parent → enfant
    for (EntityID root : scene.GetRootEntities())
        UpdateNode(scene, root, Identity(), false);
}

void TransformSystem::UpdateNode(Scene& scene, EntityID id, const Mat4& parentWorld, bool parentChanged)
{
    auto* t = scene.GetComponent<TransformComponent>(id);
    auto* h = scene.GetComponent<HierarchyComponent>(id);
    if (!t) return;

    // Matrices stockées non transposées (convention CPU) ; la transposition
    // pour HLSL est faite uniquement à l'upload GPU.
    if (t->dirty)
    {
        t->localMatrix = Scaling(t->scale) * FromQuat(t->rotation) * Translation(t->position);
    }

    const bool changed = t->dirty || parentChanged;
    if (changed)
    {
        t->worldMatrix = t->localMatrix * parentWorld;
        t->dirty = false;
    }

    if (h)
    {
        for (EntityID child : h->children)
            UpdateNode(scene, child, t->worldMatrix, changed);
    }
}
