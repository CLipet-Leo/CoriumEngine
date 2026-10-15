#pragma once

// Vue fournie hors scène (caméra éditeur), en espace monde
struct CameraView
{
    Vec3  position = { 0.f, 0.f, 0.f };
    Vec3  forward = { 0.f, 0.f, 1.f };
    Vec3  up = { 0.f, 1.f, 0.f };
    float fovY = 60.f;  // degrés
    float nearPlane = 0.1f;
    float farPlane = 1000.f;
};

class CORIUM_API CameraSystem
{
public:
    // Calcule view/proj de view si fourni, sinon de la caméra principale de la scène ;
    // remplit les constantes de scène (transposées) et retourne viewProj non transposée
    Mat4 Update(Scene& scene, float aspectRatio, SceneConstants& outScene, const CameraView* view);
};
