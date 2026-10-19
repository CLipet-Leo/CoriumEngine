#pragma once

// Modèle de binding : un espace de registres par fréquence de mise à jour (cf. Conventions.md)
//   space0 : frame    (b0 SceneConstants, b1 LightConstants)
//   space1 : matériau (b0 MaterialConstants, t0 texture, s0 sampler)
//   space2 : objet    (b0 ObjectConstants, offset dynamique)

// Vertex des meshes ; ordre des champs = ordre de VSInput dans les shaders
struct MeshVertex
{
    Vec3 position;
    Vec4 color;
    Vec2 uv;
};

// b0, space2 — données par objet, une entrée du ring buffer par draw
struct ObjectConstants
{
    Mat4 world;
    Mat4 worldViewProj;
};

// b0, space1 — données du matériau, écrites à sa création
struct MaterialConstants
{
    Vec4 baseColor; // multiplié par la couleur du vertex et la texture
};

// b0, space0 — données de scène, mis à jour une fois par frame
struct SceneConstants
{
    Mat4 viewProj;
    Vec3   cameraPos;
    float      _pad0 = 0.f;
};

// b1, space0 — lumières, mis à jour par le LightSystem
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
static_assert(sizeof(MeshVertex) == 36);
static_assert(sizeof(ObjectConstants) == 128);
static_assert(sizeof(MaterialConstants) == 16);
static_assert(sizeof(SceneConstants) == 80);
static_assert(sizeof(GpuLight) == 48);
static_assert(sizeof(LightConstants) == MAX_LIGHTS * sizeof(GpuLight) + 16);
