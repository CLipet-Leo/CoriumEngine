#pragma once

class CORIUM_API CameraSystem
{
public:
    // Calcule view/proj de la caméra principale, remplit les constantes de scène (transposées)
    // et retourne viewProj non transposée
    Mat4 Update(Scene& scene, float aspectRatio, SceneConstants& outScene);
};
