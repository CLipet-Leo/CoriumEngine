#pragma once

// register(b0) — données par objet, mis à jour avant chaque draw
struct ObjectConstants
{
    Mat4 world;
    Mat4 worldViewProj;
    Vec4   baseColor; // couleur du matériau
};

// register(b1) — données de scène, mis à jour une fois par frame
struct SceneConstants
{
    Mat4 viewProj;
    Vec3   cameraPos;
    float      _pad0 = 0.f;
};

// register(b2) — lumières, mis à jour par le LightSystem
struct GpuLight
{
    Vec3 positionOrDirection;
    float    type;        // 0=Directional, 1=Point, 2=Spot
    Vec3 color;
    float    intensity;
    float    range;
    float    spotAngle;
    Vec2 _pad;
};

constexpr uint32_t MAX_LIGHTS = 16;

struct LightConstants
{
    GpuLight lights[MAX_LIGHTS];
    uint32_t lightCount = 0;
    Vec3 _pad;
};

// Layouts identiques aux cbuffers HLSL (registres de 16 octets)
static_assert(sizeof(ObjectConstants) == 144);
static_assert(sizeof(SceneConstants) == 80);
static_assert(sizeof(GpuLight) == 48);
static_assert(sizeof(LightConstants) == MAX_LIGHTS * sizeof(GpuLight) + 16);
