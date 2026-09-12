#pragma once

#include <algorithm>
#include <cstdint>

namespace noxxa {

// GTA stores many behaviour timers as absolute CTimer millisecond values. A
// rewind must not blindly copy an old absolute timestamp into the present, or
// the timer will instantly expire. These helpers preserve the *relative*
// meaning of a historical timer and rebase it onto the new present.
inline uint32_t FutureDelayMs(uint32_t snapshotNow, uint32_t deadline, uint32_t capMs = 120000u) {
    const int32_t delta = static_cast<int32_t>(deadline - snapshotNow);
    if (delta <= 0) return 0u;
    return std::min<uint32_t>(static_cast<uint32_t>(delta), capMs);
}

inline uint32_t PastAgeMs(uint32_t snapshotNow, uint32_t eventTime, uint32_t capMs = 120000u) {
    const int32_t delta = static_cast<int32_t>(snapshotNow - eventTime);
    if (delta <= 0) return 0u;
    return std::min<uint32_t>(static_cast<uint32_t>(delta), capMs);
}

inline uint32_t RebaseDeadline(uint32_t newNow, uint32_t remainingMs) {
    return newNow + remainingMs;
}

inline uint32_t RebasePastEvent(uint32_t newNow, uint32_t ageMs) {
    return newNow - ageMs;
}

} // namespace noxxa
