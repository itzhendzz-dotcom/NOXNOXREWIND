#include "../src/CausalTime.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    using namespace noxxa;

    // Future deadline 1.5s after the historical frame remains 1.5s in the new present.
    assert(FutureDelayMs(10000u, 11500u) == 1500u);
    assert(RebaseDeadline(50000u, 1500u) == 51500u);

    // Already-expired deadlines do not become future actions after rewind.
    assert(FutureDelayMs(10000u, 9000u) == 0u);
    assert(RebaseDeadline(50000u, 0u) == 50000u);

    // A damage event that was 2s old at the selected historical frame remains 2s old.
    assert(PastAgeMs(10000u, 8000u) == 2000u);
    assert(RebasePastEvent(50000u, 2000u) == 48000u);

    // Defensive caps prevent corrupt/extreme timestamps from becoming huge timers.
    assert(FutureDelayMs(1000u, 500000u, 10000u) == 10000u);
    assert(PastAgeMs(500000u, 1000u, 10000u) == 10000u);

    std::cout << "causal_time_test: PASS\n";
    return 0;
}
