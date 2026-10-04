#include "pch.h"
#include "CameraSystem.h"

void CameraSystem::Update(Scene& scene, float aspectRatio, SceneConstants& outScene)
{
    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX proj = XMMatrixIdentity();
    XMFLOAT3 camPos = { 0.f, 0.f, 0.f };

    for (auto& [id, cam] : scene.Cameras())
    {
        if (!cam.isMain) continue;
        auto* t = scene.GetComponent<TransformComponent>(id);
        if (!t) continue;

        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(t->rotation.x),
            XMConvertToRadians(t->rotation.y),
            XMConvertToRadians(t->rotation.z));

        XMVECTOR eye = XMLoadFloat3(&t->position);
        XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
        XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), R);
        view = XMMatrixLookToLH(eye, forward, up);
        camPos = t->position;

        proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(cam.fovY), aspectRatio, cam.nearPlane, cam.farPlane);

        XMStoreFloat4x4(&cam.viewMatrix, view);
        XMStoreFloat4x4(&cam.projectionMatrix, proj);
        break;
    }

    XMStoreFloat4x4(&outScene.viewProj, XMMatrixTranspose(view * proj));
    outScene.cameraPos = camPos;
}
