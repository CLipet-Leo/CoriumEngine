#include "pch.h"
#include "TransformSystem.h"

void TransformSystem::Update(Scene& scene)
{
    // Parcours depuis les racines pour respecter l'ordre parent → enfant
    for (EntityID root : scene.GetRootEntities())
        UpdateNode(scene, root, XMMatrixIdentity(), false);
}

void TransformSystem::UpdateNode(Scene& scene, EntityID id, FXMMATRIX parentWorld, bool parentChanged)
{
    auto* t = scene.GetComponent<TransformComponent>(id);
    auto* h = scene.GetComponent<HierarchyComponent>(id);
    if (!t) return;

    // Matrices stockées non transposées (convention CPU) ; la transposition
    // pour HLSL est faite uniquement à l'upload GPU.
    if (t->dirty)
    {
        XMMATRIX S = XMMatrixScaling(t->scale.x, t->scale.y, t->scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(t->rotation.x),
            XMConvertToRadians(t->rotation.y),
            XMConvertToRadians(t->rotation.z));
        XMMATRIX T = XMMatrixTranslation(t->position.x, t->position.y, t->position.z);
        XMStoreFloat4x4(&t->localMatrix, S * R * T);
    }

    const bool changed = t->dirty || parentChanged;
    if (changed)
    {
        XMStoreFloat4x4(&t->worldMatrix, XMLoadFloat4x4(&t->localMatrix) * parentWorld);
        t->dirty = false;
    }

    if (h)
    {
        const XMMATRIX world = XMLoadFloat4x4(&t->worldMatrix);
        for (EntityID child : h->children)
            UpdateNode(scene, child, world, changed);
    }
}
