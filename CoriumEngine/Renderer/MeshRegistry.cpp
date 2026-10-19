#include "pch.h"
#include "MeshRegistry.h"

// UV des 6 sommets d'une face de BoxGeometry (bas-gauche, bas-droite, haut-droite / bas-gauche,
// haut-droite, haut-gauche).
// ponytail: propre à la box, à remplacer par les UV de la géométrie avec un vrai loader de mesh
static constexpr Vec2 kBoxFaceUV[6] = { {0, 1}, {1, 1}, {1, 0}, {0, 1}, {1, 0}, {0, 0} };

uint32_t MeshRegistry::Register(const MeshDesc& desc, RhiDevice& device)
{
    // Si déjà enregistré, retourne l'index existant
    auto it = m_nameIndex.find(desc.name);
    if (it != m_nameIndex.end())
        return it->second;

    // Box est la seule primitive pour l'instant
    const BoxGeometry geometry(desc.size);

    const auto& verts = geometry.GetVertices();
    CE_ASSERT(!verts.empty());

    // Construction des vertices GPU avec couleur procédurale
    std::vector<MeshVertex> gpuVerts;
    gpuVerts.reserve(verts.size());
    for (size_t i = 0; i < verts.size(); ++i)
    {
        const Vec3& v = verts[i];
        const float r = (v.x + 0.5f) + 0.25f;
        const float g = (v.y + 0.5f) + 0.25f;
        const float b = (v.z + 0.5f) + 0.25f;
        gpuVerts.push_back({ { v.x, v.y, v.z }, { r, g, b, 1.f }, kBoxFaceUV[i % 6] });
    }

    const auto& indices = geometry.GetIndices();
    const uint64_t vbSize = gpuVerts.size() * sizeof(MeshVertex);
    const uint64_t ibOffset = RhiAlign(vbSize, 4);
    const uint64_t ibSize = indices.size() * sizeof(uint32_t);
    const uint64_t totalSize = ibOffset + ibSize;

    GpuMesh mesh;
    mesh.desc = desc;
    mesh.vertexCount = static_cast<uint32_t>(gpuVerts.size());
    mesh.indexCount = static_cast<uint32_t>(indices.size());
    mesh.indexOffset = ibOffset;

    mesh.buffer = device.CreateBuffer({ totalSize, RhiMemory::GpuOnly,
        RhiBufferUsage_Vertex | RhiBufferUsage_Index | RhiBufferUsage_CopyDst });
    mesh.staging = device.CreateBuffer({ totalSize, RhiMemory::Upload, RhiBufferUsage_CopySrc });

    // Copie CPU → staging
    uint8_t* mapped = mesh.staging->GetMapped();
    memcpy(mapped, gpuVerts.data(), vbSize);
    if (ibSize > 0)
        memcpy(mapped + ibOffset, indices.data(), ibSize);

    uint32_t index = static_cast<uint32_t>(m_meshes.size());
    m_nameIndex[desc.name] = index;
    m_meshes.push_back(std::move(mesh));
    m_pendingUploads.push_back(index);

    return index;
}

void MeshRegistry::RecordPendingUploads(RhiCommandList* cmd, std::vector<std::unique_ptr<RhiBuffer>>& release)
{
    for (uint32_t index : m_pendingUploads)
    {
        GpuMesh& mesh = m_meshes[index];
        cmd->Barrier(mesh.buffer.get(), RhiState::Undefined, RhiState::CopyDest);
        cmd->CopyBuffer(mesh.buffer.get(), mesh.staging.get(), mesh.staging->GetSize());
        cmd->Barrier(mesh.buffer.get(), RhiState::CopyDest, RhiState::ShaderRead);

        // Même command list que les draws : la copie précède le rendu
        release.push_back(std::move(mesh.staging));
        mesh.uploaded = true;
    }
    m_pendingUploads.clear();
}

const GpuMesh* MeshRegistry::Get(uint32_t index) const
{
    if (index >= m_meshes.size()) return nullptr;
    return &m_meshes[index];
}

void MeshRegistry::Clear()
{
    m_meshes.clear();
    m_nameIndex.clear();
    m_pendingUploads.clear();
}
