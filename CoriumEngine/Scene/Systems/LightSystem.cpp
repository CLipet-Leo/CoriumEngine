#include "pch.h"
#include "LightSystem.h"

void LightSystem::Collect(Scene& scene)
{
    m_lightConstants = {};
    uint32_t count = 0;

    for (auto& [id, light] : scene.Lights())
    {
        if (!light.enabled || count >= MAX_LIGHTS) continue;

        auto* entity = scene.GetEntity(id);
        if (!entity || !entity->active) continue;

        auto* transform = scene.GetComponent<TransformComponent>(id);

        GpuLight& gpu = m_lightConstants.lights[count];

        if (light.type == LightType::Directional)
        {
            // Directionnelle : axe Z monde de l'entité
            if (transform)
                gpu.positionOrDirection = Normalize(GetRow(transform->worldMatrix, 2));
        }
        else
        {
            // Point/spot : position monde
            if (transform)
                gpu.positionOrDirection = GetRow(transform->worldMatrix, 3);
        }

        gpu.type = static_cast<float>(light.type);
        gpu.color = light.color;
        gpu.intensity = light.intensity;
        gpu.range = light.range;
        gpu.spotAngle = light.spotAngle * kDegToRad;

        ++count;
    }

    m_lightConstants.lightCount = count;
}

void LightSystem::Upload(UINT8* mappedBuffer)
{
    CE_ASSERT(mappedBuffer != nullptr);
    memcpy(mappedBuffer, &m_lightConstants, sizeof(LightConstants));
}