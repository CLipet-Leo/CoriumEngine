#pragma once

class CORIUM_API CameraSystem
{
public:
    // Calcule view/proj de view si fourni, sinon de la caméra principale de la scène ;
    // remplit les constantes de scène (transposées) et retourne viewProj non transposée
    Mat4 Update(Scene& scene, float aspectRatio, SceneConstants& outScene, const CameraView* view);
};
