#pragma once

// Temps de frame : Tick() une fois par frame, avant toute lecture
class Clock
{
public:
    using Steady = std::chrono::steady_clock;

    // Borne du dt : un breakpoint ou un déplacement de fenêtre ne fait pas « sauter » la simulation
    static constexpr float kMaxDeltaTime = 0.1f;

    void Tick()
    {
        const Steady::time_point now = Steady::now();
        m_deltaTime = (std::min)(std::chrono::duration<float>(now - m_last).count(), kMaxDeltaTime);
        m_last = now;
    }

    // Secondes écoulées entre les deux derniers Tick, bornées par kMaxDeltaTime
    float DeltaTime() const { return m_deltaTime; }

    // Secondes réelles écoulées entre la création et le dernier Tick (non bornées)
    double TotalTime() const { return std::chrono::duration<double>(m_last - m_start).count(); }

private:
    Steady::time_point m_start = Steady::now();
    Steady::time_point m_last = m_start;
    float              m_deltaTime = 0.f;
};
