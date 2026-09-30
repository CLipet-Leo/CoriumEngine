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
            // Pour les directionnelles, on encode la direction depuis la rotation
            if (transform)
            {
                // Direction dérivée de la rotation (axe Z local transformé)
                XMMATRIX R = XMMatrixRotationRollPitchYaw(
                    XMConvertToRadians(transform->rotation.x),
                    XMConvertToRadians(transform->rotation.y),
                    XMConvertToRadians(transform->rotation.z));
                XMVECTOR dir = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
                XMStoreFloat3(&gpu.positionOrDirection, dir);
            }
        }
        else
        {
            // Pour point/spot : on encode la position
            if (transform)
                gpu.positionOrDirection = transform->position;
        }

        gpu.type = static_cast<float>(light.type);
        gpu.color = light.color;
        gpu.intensity = light.intensity;
        gpu.range = light.range;
        gpu.spotAngle = XMConvertToRadians(light.spotAngle);

        ++count;
    }

    m_lightConstants.lightCount = count;
}

void LightSystem::Upload(UINT8* mappedBuffer)
{
    CE_ASSERT(mappedBuffer != nullptr);
    memcpy(mappedBuffer, &m_lightConstants, sizeof(LightConstants));
}