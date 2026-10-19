#pragma once

// Caméra libre de l'éditeur, hors scène (jamais sérialisée) :
// clic droit maintenu + souris pour regarder, ZQSD pour se déplacer, A/E pour descendre/monter, Shift pour accélérer
class EditorCamera
{
public:
    // uiWantsMouse : un clic droit commencé sur un panneau de l'UI n'active pas la caméra
    void Update(float dt, const Input& input, bool uiWantsMouse);

    CameraView GetView() const;

private:
    Mat4 Rotation() const;

    Vec3  m_position = { 0.f, 1.5f, -4.f };
    float m_yaw = 0.f;   // degrés
    float m_pitch = 0.f; // degrés, borné à ±89 (pas de retournement)
    bool  m_looking = false;
    bool  m_rightWasDown = false;
};
