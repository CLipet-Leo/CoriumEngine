#pragma once

class CORIUM_API RenderSystem
{
public:
    void Init(ID3D12Device* device, uint32_t frameCount);

    // Soumet tous les draw calls visibles
    void Submit(Scene& scene,
        ID3D12GraphicsCommandList* cmdList,
        uint32_t                   frameIndex,
        const MeshRegistry& meshRegistry,
        const MaterialRegistry& materialRegistry,
        const SceneConstants& sceneData);

private:
    // Alloue un constant buffer persistant pour les données par objet
    void CreateObjectConstantBuffer(ID3D12Device* device, uint32_t frameCount);

    // Calcule et upload les données de la caméra principale
    bool UpdateCameraData(Scene& scene, SceneConstants& outScene);

    // Ring buffer de CBVs pour les données par-objet (1 slot par draw par frame)
    static constexpr uint32_t MAX_OBJECTS_PER_FRAME = 512;

    struct FrameResources
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> objectCB;
        UINT8* mapped = nullptr;
        uint32_t                               usedSlots = 0;
    };

    std::vector<FrameResources> m_frameResources;
    uint32_t                    m_objectCBAlignedSize = 0;
};