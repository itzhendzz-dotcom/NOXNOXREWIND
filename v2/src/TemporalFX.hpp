#pragma once

#include <chrono>

namespace noxxa {

class RewindCore;

class TemporalFX {
public:
    void Draw(const RewindCore& core);

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_epoch{Clock::now()};
};

} // namespace noxxa
