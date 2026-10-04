#pragma once

class CORIUM_API CameraSystem
{
public:
    // Calcule view/proj de la caméra principale et remplit les constantes de scène
    void Update(Scene& scene, float aspectRatio, SceneConstants& outScene);
};
