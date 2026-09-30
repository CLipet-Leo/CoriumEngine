#pragma once

struct GpuMesh
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer; // VB puis IB dans la même ressource
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer; // gardé vivant jusqu'à la fin de la copie GPU
    D3D12_VERTEX_BUFFER_VIEW               vbView = {};
    D3D12_INDEX_BUFFER_VIEW                ibView = {};
    uint32_t                               vertexCount = 0;
    uint32_t                               indexCount = 0;
    uint32_t                               sizeInBytes = 0;
    uint64_t                               uploadFenceValue = 0;
    std::string                            name;
    bool                                   uploaded = false;
};

class CORIUM_API MeshRegistry
{
public:
    MeshRegistry() = default;

    // Retourne l'index du mesh, crée si inexistant.
    // La copie GPU est différée : elle sera enregistrée par RecordPendingUploads.
    uint32_t Register(const std::string& name, const BufferGeometry& geometry,
        ID3D12Device* device);

    const GpuMesh* Get(uint32_t index) const;
    bool           IsValid(uint32_t index) const;

    // Enregistre les copies en attente dans la command list de la frame.
    // fenceValue : valeur de fence qui sera signalée à la fin de cette frame.
    void RecordPendingUploads(ID3D12GraphicsCommandList* cmdList, uint64_t fenceValue);

    // Libère les upload buffers dont la copie est terminée côté GPU
    void ReleaseCompletedUploads(uint64_t completedFenceValue);

    uint32_t Count() const { return static_cast<uint32_t>(m_meshes.size()); }

private:
    std::vector<GpuMesh>                   m_meshes;
    std::unordered_map<std::string, uint32_t> m_nameIndex;
    std::vector<uint32_t>                  m_pendingUploads;
    std::vector<uint32_t>                  m_inFlightUploads;
};