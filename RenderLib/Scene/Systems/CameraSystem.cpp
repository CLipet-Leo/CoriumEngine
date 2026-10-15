#include "pch.h"
#include "CameraSystem.h"

// Caméra isMain de la scène, si elle existe
static std::optional<CameraView> FindMainCamera(Scene& scene)
{
    for (auto& [id, cam] : scene.Cameras())
    {
        if (!cam.isMain) continue;
        auto* t = scene.GetComponent<TransformComponent>(id);
        if (!t) continue;

        // Repère monde de la caméra (hiérarchie incluse) : ligne 1 = up, 2 = forward, 3 = position
        const Mat4& w = t->worldMatrix;
        return CameraView{ GetRow(w, 3), GetRow(w, 2), GetRow(w, 1), cam.fovY, cam.nearPlane, cam.farPlane };
    }
    return std::nullopt;
}

Mat4 CameraSystem::Update(Scene& scene, float aspectRatio, SceneConstants& outScene, const CameraView* view)
{
    Mat4 viewProj = Identity();
    Vec3 camPos = { 0.f, 0.f, 0.f };

    const std::optional<CameraView> cam = view ? std::optional<CameraView>(*view) : FindMainCamera(scene);
    if (cam)
    {
        camPos = cam->position;
        viewProj = LookToLH(cam->position, cam->forward, cam->up)
                 * PerspectiveFovLH(cam->fovY * kDegToRad, aspectRatio, cam->nearPlane, cam->farPlane);
    }

    outScene.viewProj = Transpose(viewProj);
    outScene.cameraPos = camPos;
    return viewProj;
}
