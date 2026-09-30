#include "pch.h"
#include "RenderSystem.h"

void RenderSystem::Init(ID3D12Device* device, uint32_t frameCount)
{
    CreateObjectConstantBuffer(device, frameCount);
}

void RenderSystem::CreateObjectConstantBuffer(ID3D12Device* device, uint32_t frameCount)
{
    // Les CBV doivent être alignés sur 256 bytes
    m_objectCBAlignedSize = (sizeof(ObjectConstants) + 255) & ~255;
    const UINT totalSize = m_objectCBAlignedSize * MAX_OBJECTS_PER_FRAME;

    m_frameResources.resize(frameCount);

    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSize);

    for (auto& fr : m_frameResources)
    {
        EVAL_HR(device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&fr.objectCB)),
            "RenderSystem: object constant buffer creation failed");

        EVAL_HR(fr.objectCB->Map(0, nullptr, reinterpret_cast<void**>(&fr.mapped)),
            "RenderSystem: object CB map failed");
    }
}

void RenderSystem::Submit(Scene& scene, ID3D12GraphicsCommandList* cmdList,
    uint32_t frameIndex, const MeshRegistry& meshRegistry,
    const MaterialRegistry& materialRegistry,
    const SceneConstants& sceneData)
{
    auto& fr = m_frameResources[frameIndex];
    fr.usedSlots = 0;
    ID3D12PipelineState* boundPso = nullptr;

    // Précalcul de ViewProj depuis sceneData
    // sceneData.viewProj est stockée transposée pour le GPU
    XMMATRIX viewProj = XMMatrixTranspose(XMLoadFloat4x4(&sceneData.viewProj));

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
        if (!material || !material->pso) continue;

        if (material->pso.Get() != boundPso)
        {
            boundPso = material->pso.Get();
            cmdList->SetPipelineState(boundPso);
        }

        CE_ASSERT(fr.usedSlots < MAX_OBJECTS_PER_FRAME);

        // Construction des données par objet
        ObjectConstants objData;
        const XMMATRIX world = XMLoadFloat4x4(&transform->worldMatrix);

        XMStoreFloat4x4(&objData.world, XMMatrixTranspose(world));
        XMStoreFloat4x4(&objData.worldViewProj, XMMatrixTranspose(world * viewProj));
        objData.baseColor = material->desc.baseColor;

        // Upload dans le slot du ring buffer
        const UINT offset = fr.usedSlots * m_objectCBAlignedSize;
        memcpy(fr.mapped + offset, &objData, sizeof(ObjectConstants));

        // Liaison des CBVs et draw call
        const D3D12_GPU_VIRTUAL_ADDRESS objCBAddr =
            fr.objectCB->GetGPUVirtualAddress() + offset;

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddr);
        cmdList->IASetVertexBuffers(0, 1, &gpuMesh->vbView);
        if (gpuMesh->indexCount > 0)
        {
            cmdList->IASetIndexBuffer(&gpuMesh->ibView);
            cmdList->DrawIndexedInstanced(gpuMesh->indexCount, 1, 0, 0, 0);
        }
        else
        {
            cmdList->DrawInstanced(gpuMesh->vertexCount, 1, 0, 0);
        }

        ++fr.usedSlots;
    }
}