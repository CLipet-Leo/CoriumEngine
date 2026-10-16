cbuffer ObjectCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldViewProj;
    float4 gBaseColor;
};

cbuffer SceneCB : register(b1)
{
    float4x4 gViewProj;
    float3 gCameraPos;
    float _pad0;
};

cbuffer LightCB : register(b2)
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

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 1.f), gWorldViewProj);
    o.color = input.color * gBaseColor;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}