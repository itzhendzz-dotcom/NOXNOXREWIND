#include "../src/TimelineBuffer.hpp"
#include <cassert>
#include <iostream>

struct F { int v; };

int main() {
    using noxxa::TimelineBuffer;

    // Ring wrap keeps the newest logical history in order.
    TimelineBuffer<F> t(4);
    t.Push({1}); t.Push({2}); t.Push({3}); t.Push({4}); t.Push({5});
    assert(t.Size() == 4);
    assert(t.Front().v == 2);
    assert(t.Back().v == 5);

    // Cancel must leave the future untouched.
    assert(t.BeginRewind());
    t.StepBack();
    t.StepBack();
    assert(t.Current()->v == 3);
    t.CancelRewind();
    assert(!t.IsRewinding());
    assert(t.Back().v == 5);

    // Commit branches the timeline and discards the abandoned future.
    assert(t.BeginRewind());
    t.StepBack();
    t.StepBack();
    assert(t.Current()->v == 3);
    t.CommitRewind();
    assert(t.Size() == 2);
    assert(t.Back().v == 3);
    t.Push({9});
    assert(t.Back().v == 9);

    // Resizing preserves the newest logical samples and resets rewind state.
    t.Push({10}); t.Push({11});
    t.SetCapacity(3);
    assert(t.Capacity() == 3);
    assert(t.Size() == 3);
    assert(t.Front().v == 9);
    assert(t.Back().v == 11);
    assert(!t.IsRewinding());

    // Boundary stepping never underflows the cursor.
    assert(t.BeginRewind());
    assert(t.StepBack()->v == 10);
    assert(t.StepBack()->v == 9);
    assert(!t.CanStepBack());
    assert(t.StepBack()->v == 9);
    t.CommitRewind();
    assert(t.Size() == 1);
    assert(t.Back().v == 9);

    std::cout << "timeline_test_v3: PASS\n";
    return 0;
}
