// Modèle de binding : un espace par fréquence de mise à jour (cf. ShaderTypes.h, Conventions.md).
// En SPIR-V : spaceN = set N ; b → binding N, t → N + 10, s → N + 20 (options DXC du .vcxproj).

// --- space0 : frame ---
cbuffer SceneCB : register(b0, space0)
{
    float4x4 gViewProj;
    float3 gCameraPos;
    float _pad0;
};

cbuffer LightCB : register(b1, space0)
{
    struct GpuLight
    {
        float3 posDir;
        float type;
        float3 color;
        float intensity;
        float range;
        float spotAngle;
        float2 _pad;
    } gLights[16];
    uint gLightCount;
    float3 _pad1;
};

// --- space1 : matériau ---
cbuffer MaterialCB : register(b0, space1)
{
    float4 gBaseColor;
};
Texture2D    gAlbedo  : register(t0, space1);
SamplerState gSampler : register(s0, space1);

// --- space2 : objet ---
cbuffer ObjectCB : register(b0, space2)
{
    float4x4 gWorld;
    float4x4 gWorldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 1.f), gWorldViewProj);
    o.color = input.color * gBaseColor;
    o.uv = input.uv;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color * gAlbedo.Sample(gSampler, input.uv);
}
