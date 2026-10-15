#include "stdafx.h"

namespace
{
    constexpr float kSpeed = 4.f;          // unités par seconde
    constexpr float kFastFactor = 4.f;     // avec Shift
    constexpr float kSensitivity = 0.15f;  // degrés par count souris
}

void EditorCamera::Update(float dt, const Input& input, bool uiWantsMouse)
{
    // Le mode « regard » ne démarre que sur l'appui, pour ne pas s'activer quand un drag
    // commencé dans l'UI sort du panneau ; il ignore ensuite l'UI jusqu'au relâchement.
    const bool rightDown = input.IsDown(VK_RBUTTON);
    if (rightDown && !m_rightWasDown && !uiWantsMouse)
        m_looking = true;
    if (!rightDown)
        m_looking = false;
    m_rightWasDown = rightDown;

    if (!m_looking)
        return;

    const Vec2 delta = input.MouseDelta();
    m_yaw += delta.x * kSensitivity;
    m_pitch = std::clamp(m_pitch + delta.y * kSensitivity, -89.f, 89.f);

    const Mat4 r = Rotation();
    const Vec3 right = GetRow(r, 0);
    const Vec3 forward = GetRow(r, 2);
    const Vec3 worldUp = { 0.f, 1.f, 0.f };

    Vec3 move = { 0.f, 0.f, 0.f };
    // Codes virtuels = touches telles que libellées par la disposition active (ici AZERTY)
    if (input.IsDown('Z')) move = move + forward;
    if (input.IsDown('S')) move = move - forward;
    if (input.IsDown('D')) move = move + right;
    if (input.IsDown('Q')) move = move - right;
    if (input.IsDown('E')) move = move + worldUp;
    if (input.IsDown('A')) move = move - worldUp;

    if (Dot(move, move) > 0.f)
    {
        const float speed = kSpeed * (input.IsDown(VK_SHIFT) ? kFastFactor : 1.f);
        m_position = m_position + Normalize(move) * (speed * dt);
    }
}

CameraView EditorCamera::GetView() const
{
    const Mat4 r = Rotation();
    CameraView view;
    view.position = m_position;
    view.forward = GetRow(r, 2);
    view.up = GetRow(r, 1);
    return view;
}

Mat4 EditorCamera::Rotation() const
{
    return FromQuat(FromEulerDeg({ m_pitch, m_yaw, 0.f }));
}
