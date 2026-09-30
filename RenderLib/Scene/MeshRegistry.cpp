#include "pch.h"
#include "MeshRegistry.h"
#include "../Core/Geometry/Types.h"

// Vertex GPU — position + couleur générée proceduralement
struct MeshVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

uint32_t MeshRegistry::Register(const std::string& name, const BufferGeometry& geometry,
    ID3D12Device* device)
{
    // Si déjà enregistré, retourne l'index existant
    auto it = m_nameIndex.find(name);
    if (it != m_nameIndex.end())
        return it->second;

    const auto& verts = geometry.GetVertices();
    CE_ASSERT(!verts.empty());

    // Construction des vertices GPU avec couleur procedurale
    std::vector<MeshVertex> gpuVerts;
    gpuVerts.reserve(verts.size());
    for (const auto& v : verts)
    {
        const float r = (v.x + 0.5f) + 0.25f;
        const float g = (v.y + 0.5f) + 0.25f;
        const float b = (v.z + 0.5f) + 0.25f;
        gpuVerts.push_back({ { v.x, v.y, v.z }, { r, g, b, 1.f } });
    }

    const auto& indices = geometry.GetIndices();
    const UINT vbSize = static_cast<UINT>(gpuVerts.size() * sizeof(MeshVertex));
    const UINT ibOffset = (vbSize + 3u) & ~3u;
    const UINT ibSize = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    const UINT totalSize = ibOffset + ibSize;

    GpuMesh mesh;
    mesh.name = name;
    mesh.vertexCount = static_cast<uint32_t>(gpuVerts.size());
    mesh.indexCount = static_cast<uint32_t>(indices.size());

    // Heap DEFAULT — réside sur le GPU, performances optimales
    auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSize);

    EVAL_HR(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&mesh.vertexBuffer)),
        "MeshRegistry: vertex buffer (default heap) creation failed");

    // Upload buffer intermédiaire
    EVAL_HR(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mesh.uploadBuffer)),
        "MeshRegistry: upload buffer creation failed");

    // Copie CPU → upload buffer
    UINT8* mapped = nullptr;
    EVAL_HR(mesh.uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped)),
        "MeshRegistry: upload buffer map failed");
    memcpy(mapped, gpuVerts.data(), vbSize);
    if (ibSize > 0)
        memcpy(mapped + ibOffset, indices.data(), ibSize);
    mesh.uploadBuffer->Unmap(0, nullptr);

    const D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vbView.BufferLocation = gpuAddr;
    mesh.vbView.StrideInBytes = sizeof(MeshVertex);
    mesh.vbView.SizeInBytes = vbSize;
    if (ibSize > 0)
    {
        mesh.ibView.BufferLocation = gpuAddr + ibOffset;
        mesh.ibView.Format = DXGI_FORMAT_R32_UINT;
        mesh.ibView.SizeInBytes = ibSize;
    }
    mesh.sizeInBytes = totalSize;
    mesh.uploaded = false;

    uint32_t index = static_cast<uint32_t>(m_meshes.size());
    m_nameIndex[name] = index;
    m_meshes.push_back(std::move(mesh));
    m_pendingUploads.push_back(index);

    return index;
}

void MeshRegistry::RecordPendingUploads(ID3D12GraphicsCommandList* cmdList, uint64_t fenceValue)
{
    if (m_pendingUploads.empty()) return;

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(m_pendingUploads.size());

    for (uint32_t index : m_pendingUploads)
    {
        GpuMesh& mesh = m_meshes[index];
        cmdList->CopyBufferRegion(mesh.vertexBuffer.Get(), 0,
            mesh.uploadBuffer.Get(), 0, mesh.sizeInBytes);

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            mesh.vertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER));

        // Même command list que les draws : la copie précède le rendu
        mesh.uploaded = true;
        mesh.uploadFenceValue = fenceValue;
        m_inFlightUploads.push_back(index);
    }

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    m_pendingUploads.clear();
}

void MeshRegistry::ReleaseCompletedUploads(uint64_t completedFenceValue)
{
    std::erase_if(m_inFlightUploads, [&](uint32_t index)
    {
        GpuMesh& mesh = m_meshes[index];
        if (mesh.uploadFenceValue > completedFenceValue) return false;
        mesh.uploadBuffer.Reset();
        return true;
    });
}

const GpuMesh* MeshRegistry::Get(uint32_t index) const
{
    if (index >= m_meshes.size()) return nullptr;
    return &m_meshes[index];
}

bool MeshRegistry::IsValid(uint32_t index) const
{
    return index < m_meshes.size() && m_meshes[index].uploaded;
}
