#include "pch.h"
#include "RenderSystem.h"

void RenderSystem::Init(RhiDevice& device, RhiPipelineLayout* layout)
{
    for (auto& fr : m_frames)
    {
        fr.objectCB = device.CreateBuffer({ SLOT_SIZE * MAX_OBJECTS_PER_FRAME, RhiMemory::Upload,
            RhiBufferUsage_Constant });
        fr.bindings = device.CreateBindingSet({ layout, 2, { { fr.objectCB.get(), 0, SLOT_SIZE } } });
    }
}

void RenderSystem::Shutdown()
{
    for (auto& fr : m_frames)
        fr = {};
}

void RenderSystem::Submit(Scene& scene, RhiCommandList* cmd,
    uint32_t frameIndex, RhiBindingSet* frameSet,
    const MeshRegistry& meshRegistry,
    const MaterialRegistry& materialRegistry,
    const Mat4& viewProj)
{
    auto& fr = m_frames[frameIndex];
    uint32_t usedSlots = 0;
    RhiPipeline* boundPipeline = nullptr;
    const Material* boundMaterial = nullptr;

    for (auto& [id, mesh] : scene.Meshes())
    {
        if (!mesh.visible) continue;

        auto* entity = scene.GetEntity(id);
        if (!entity || !entity->active) continue;

        const GpuMesh* gpuMesh = meshRegistry.Get(mesh.meshIndex);
        if (!gpuMesh || !gpuMesh->uploaded) continue;

        auto* transform = scene.GetComponent<TransformComponent>(id);
        if (!transform) continue;

        const Material* material = materialRegistry.Get(mesh.materialIndex);
        if (!material) material = materialRegistry.Get(0);
        if (!material || !material->pipeline) continue;

        if (usedSlots >= MAX_OBJECTS_PER_FRAME)
        {
            CE_ASSERT(false && "MAX_OBJECTS_PER_FRAME atteint");
            break;
        }

        if (material->pipeline != boundPipeline)
        {
            cmd->SetPipeline(material->pipeline);
            if (!boundPipeline)
                cmd->SetBindingSet(frameSet);
            boundPipeline = material->pipeline;
        }
        if (material != boundMaterial)
        {
            cmd->SetBindingSet(material->bindings.get());
            boundMaterial = material;
        }

        // Données par objet dans le slot du ring buffer
        ObjectConstants objData;
        const Mat4& world = transform->worldMatrix;
        objData.world = Transpose(world);
        objData.worldViewProj = Transpose(world * viewProj);

        const uint32_t offset = usedSlots * SLOT_SIZE;
        memcpy(fr.objectCB->GetMapped() + offset, &objData, sizeof(ObjectConstants));
        cmd->SetBindingSet(fr.bindings.get(), offset);

        cmd->SetVertexBuffer(gpuMesh->buffer.get(), 0, sizeof(MeshVertex));
        if (gpuMesh->indexCount > 0)
        {
            cmd->SetIndexBuffer(gpuMesh->buffer.get(), gpuMesh->indexOffset);
            cmd->DrawIndexed(gpuMesh->indexCount);
        }
        else
        {
            cmd->Draw(gpuMesh->vertexCount);
        }

        ++usedSlots;
    }
}
