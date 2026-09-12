#pragma once

#include <chrono>

namespace noxxa {
class RewindCore;

class TemporalFX {
public:
    void Init(float strength) { m_strength = strength; }
    void Draw(const RewindCore& core);

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_epoch{Clock::now()};
    float m_strength{1.0f};
};
} // namespace noxxa
