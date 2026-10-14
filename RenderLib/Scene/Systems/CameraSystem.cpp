#include "pch.h"
#include "CameraSystem.h"

Mat4 CameraSystem::Update(Scene& scene, float aspectRatio, SceneConstants& outScene)
{
    Mat4 view = Identity();
    Mat4 proj = Identity();
    Vec3 camPos = { 0.f, 0.f, 0.f };

    for (auto& [id, cam] : scene.Cameras())
    {
        if (!cam.isMain) continue;
        auto* t = scene.GetComponent<TransformComponent>(id);
        if (!t) continue;

        // Repère monde de la caméra (hiérarchie incluse) : ligne 1 = up, 2 = forward, 3 = position
        const Mat4& w = t->worldMatrix;
        camPos = GetRow(w, 3);
        view = LookToLH(camPos, GetRow(w, 2), GetRow(w, 1));

        proj = PerspectiveFovLH(cam.fovY * kDegToRad, aspectRatio, cam.nearPlane, cam.farPlane);

        break;
    }

    const Mat4 viewProj = view * proj;
    outScene.viewProj = Transpose(viewProj);
    outScene.cameraPos = camPos;
    return viewProj;
}
