#include "../src/TimelineBuffer.hpp"
#include <cassert>
#include <iostream>

struct F { int v; };

int main() {
    noxxa::TimelineBuffer<F> t(4);
    t.Push({1}); t.Push({2}); t.Push({3}); t.Push({4}); t.Push({5});
    assert(t.Size() == 4);
    assert(t.Front().v == 2);
    assert(t.Back().v == 5);
    assert(t.BeginRewind());
    assert(t.Current()->v == 5);
    assert(t.PeekStepBack()->v == 4);
    t.StepBack();
    assert(t.Current()->v == 4);
    t.StepBack();
    assert(t.Current()->v == 3);
    t.CommitRewind();
    assert(!t.IsRewinding());
    assert(t.Back().v == 3);
    t.Push({9});
    assert(t.Back().v == 9);
    std::cout << "timeline_test: PASS\n";
    return 0;
}
