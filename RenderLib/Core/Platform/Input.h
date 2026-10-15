#pragma once

// État brut clavier/souris, alimenté par le WndProc de l'application.
// Aucune connaissance de l'UI : le filtrage (ImGui, viewport) est décidé par l'appelant.
class CORIUM_API Input
{
public:
    // Active le Raw Input souris (delta relatif, insensible aux bords de l'écran)
    void Init(HWND hWnd);

    // À appeler pour chaque message de la fenêtre, sans le consommer
    void OnMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Fin de frame : remet à zéro le delta souris accumulé
    void EndFrame();

    // Relâche tout (perte de focus : les WM_KEYUP n'arriveraient jamais)
    void Reset();

    // vk = code virtuel Win32 ; boutons souris via VK_LBUTTON / VK_RBUTTON / VK_MBUTTON
    bool IsDown(uint8_t vk) const { return m_down[vk]; }

    // Déplacement souris depuis la dernière EndFrame, en counts (≈ pixels)
    Vec2 MouseDelta() const { return m_mouseDelta; }

private:
    std::array<bool, 256> m_down = {};
    Vec2                  m_mouseDelta = { 0.f, 0.f };
};
