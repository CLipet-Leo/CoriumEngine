#pragma once

class CORIUM_API LightSystem
{
public:
    // Collecte les lumières actives de la scène dans m_lightConstants
    void Collect(Scene& scene);

    // Upload vers le constant buffer GPU
    void Upload(UINT8* mappedBuffer);

    const LightConstants& GetData()  const { return m_lightConstants; }
    uint32_t              GetCount() const { return m_lightConstants.lightCount; }

private:
    LightConstants m_lightConstants = {};
};