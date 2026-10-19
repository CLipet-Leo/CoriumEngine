#pragma once

struct GpuMesh
{
    std::unique_ptr<RhiBuffer> buffer;  // VB puis IB dans le même buffer
    std::unique_ptr<RhiBuffer> staging; // jusqu'à l'enregistrement de la copie
    uint64_t                   indexOffset = 0;
    uint32_t                   vertexCount = 0;
    uint32_t                   indexCount = 0;
    MeshDesc                   desc;
    bool                       uploaded = false;
};

class MeshRegistry
{
public:
    // Retourne l'index du mesh, crée si inexistant (un nom déjà enregistré garde ses paramètres d'origine).
    // La copie GPU est différée : elle sera enregistrée par RecordPendingUploads.
    uint32_t Register(const MeshDesc& desc, RhiDevice& device);

    const GpuMesh* Get(uint32_t index) const;

    // Enregistre les copies en attente dans la command list de la frame ; les buffers de staging
    // sont déplacés dans release, à libérer quand la frame est terminée sur le GPU
    void RecordPendingUploads(RhiCommandList* cmd, std::vector<std::unique_ptr<RhiBuffer>>& release);

    void Clear();

private:
    std::vector<GpuMesh>                      m_meshes;
    std::unordered_map<std::string, uint32_t> m_nameIndex;
    std::vector<uint32_t>                     m_pendingUploads;
};
