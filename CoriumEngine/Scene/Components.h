#pragma once

// -------------------------------------------------------
// Transform — position, rotation, scale + matrice world
// -------------------------------------------------------
struct TransformComponent
{
    Vec3 position = { 0.f, 0.f, 0.f };
    Quat rotation = { 0.f, 0.f, 0.f, 1.f };
    Vec3 scale = { 1.f, 1.f, 1.f };

    // Calculé par le TransformSystem — ne pas modifier manuellement
    Mat4 localMatrix = {};
    Mat4 worldMatrix = {};
    bool        dirty = true; // true = recalcul nécessaire

    Mat4 GetWorldMatrix() const
    {
        return worldMatrix;
    }

    void SetPosition(float x, float y, float z)
    {
        position = { x, y, z };
        dirty = true;
    }

    // euler = (pitch, yaw, roll) en degrés, cf. FromEulerDeg
    void SetRotationDeg(const Vec3& euler)
    {
        rotation = FromEulerDeg(euler);
        dirty = true;
    }
};

// -------------------------------------------------------
// Hierarchy — relation parent / enfants
// -------------------------------------------------------
struct HierarchyComponent
{
    EntityID              parent = NULL_ENTITY;
    std::vector<EntityID> children = {};

    bool HasParent() const { return parent != NULL_ENTITY; }
};

// -------------------------------------------------------
// Mesh — référence vers une ressource GPU
// -------------------------------------------------------
struct MeshComponent
{
    uint32_t meshIndex = UINT32_MAX; // index dans le MeshRegistry
    uint32_t materialIndex = 0;
    bool     castShadows = true;
    bool     visible = true;
};

// -------------------------------------------------------
// Camera — paramètres de projection
// -------------------------------------------------------
struct CameraComponent
{
    float fovY = 60.f;  // degrés
    float nearPlane = 0.1f;
    float farPlane = 1000.f;
    bool  isMain = false; // seule la caméra principale est utilisée
};

// -------------------------------------------------------
// Light — point, directionnel, spot
// -------------------------------------------------------
enum class LightType : uint8_t { Directional, Point, Spot };

struct LightComponent
{
    LightType type = LightType::Point;
    Vec3  color = { 1.f, 1.f, 1.f };
    float     intensity = 1.f;
    float     range = 10.f;   // pour Point et Spot
    float     spotAngle = 30.f;   // pour Spot, en degrés
    bool      enabled = true;
};
