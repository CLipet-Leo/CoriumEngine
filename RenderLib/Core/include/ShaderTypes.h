#pragma once

using namespace DirectX;

// register(b0) — données par objet, mis à jour avant chaque draw
struct ObjectConstants
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 worldViewProj;
    XMFLOAT4   baseColor; // couleur du matériau
};

// register(b1) — données de scène, mis à jour une fois par frame
struct SceneConstants
{
    XMFLOAT4X4 viewProj;
    XMFLOAT3   cameraPos;
    float      _pad0 = 0.f;
};

// register(b2) — lumières, mis à jour par le LightSystem
struct GpuLight
{
    XMFLOAT3 positionOrDirection;
    float    type;        // 0=Directional, 1=Point, 2=Spot
    XMFLOAT3 color;
    float    intensity;
    float    range;
    float    spotAngle;
    XMFLOAT2 _pad;
};

constexpr uint32_t MAX_LIGHTS = 16;

struct LightConstants
{
    GpuLight lights[MAX_LIGHTS];
    uint32_t lightCount = 0;
    XMFLOAT3 _pad;
};