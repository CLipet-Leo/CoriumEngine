#pragma once

class RenderSystem
{
public:
    // layout : espace 2 = constantes par objet (constant buffer dynamique)
    void Init(RhiDevice& device, RhiPipelineLayout* layout);
    void Shutdown();

    // Soumet tous les draw calls visibles, dans une passe ouverte.
    // frameSet (space0) est lié avec le premier pipeline.
    void Submit(Scene& scene,
        RhiCommandList*         cmd,
        uint32_t                frameIndex,
        RhiBindingSet*          frameSet,
        const MeshRegistry&     meshRegistry,
        const MaterialRegistry& materialRegistry,
        const Mat4&             viewProj);

private:
    // Ring buffer par frame : un slot de constantes par draw, choisi par offset dynamique
    static constexpr uint32_t MAX_OBJECTS_PER_FRAME = 512;
    static constexpr uint32_t SLOT_SIZE = (uint32_t)RhiAlign(sizeof(ObjectConstants), kRhiConstantAlign);

    struct FrameResources
    {
        std::unique_ptr<RhiBuffer>     objectCB;
        std::unique_ptr<RhiBindingSet> bindings; // space2
    };
    std::array<FrameResources, kRhiFramesInFlight> m_frames;
};
