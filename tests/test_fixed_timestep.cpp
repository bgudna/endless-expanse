#include <doctest/doctest.h>

#include "core/FixedTimestep.h"

#include <cstdlib>

using ee::FixedTimestep;

namespace {
// Simulate `seconds` of wall time at a given render rate; return total sim steps.
int stepsFor(double seconds, double renderHz) {
    FixedTimestep ts(1.0 / 60.0);
    const double frame = 1.0 / renderHz;
    const long frames = static_cast<long>(seconds * renderHz + 0.5);
    int total = 0;
    for (long i = 0; i < frames; ++i) total += ts.advance(frame);
    return total;
}
} // namespace

TEST_CASE("sim step count is independent of render rate") {
    for (double hz : {30.0, 60.0, 75.0, 120.0, 144.0, 240.0}) {
        INFO("render rate = " << hz);
        // Allow +-1 step for accumulator remainder.
        CHECK(std::abs(stepsFor(10.0, hz) - 600) <= 1);
    }
}

TEST_CASE("slow frames run multiple steps") {
    FixedTimestep ts(1.0 / 60.0);
    CHECK(ts.advance(1.0 / 20.0) == 3);
}

TEST_CASE("fast frames may run zero steps and accumulate") {
    FixedTimestep ts(1.0 / 60.0);
    CHECK(ts.advance(1.0 / 240.0) == 0);
    CHECK(ts.alpha() > 0.0);
    CHECK(ts.alpha() < 1.0);
}

TEST_CASE("spiral of death is clamped") {
    FixedTimestep ts(1.0 / 60.0, 0.25);
    // A 10 second hitch must not produce 600 steps.
    CHECK(ts.advance(10.0) == 15);
}

TEST_CASE("negative frame time is ignored") {
    FixedTimestep ts(1.0 / 60.0);
    CHECK(ts.advance(-1.0) == 0);
}
