#pragma once

namespace ee {

// Fixed-timestep accumulator. Feed it real frame times; it tells you how many
// fixed simulation steps to run. alpha() is the render interpolation factor.
class FixedTimestep {
public:
    explicit FixedTimestep(double stepSeconds = 1.0 / 60.0, double maxFrameSeconds = 0.25)
        : step_(stepSeconds), maxFrame_(maxFrameSeconds) {}

    // Returns the number of simulation steps to run for this frame.
    // Frame time is clamped to maxFrame_ (spiral-of-death guard).
    int advance(double frameSeconds);

    double step() const { return step_; }
    double alpha() const { return accumulator_ / step_; }
    void reset() { accumulator_ = 0.0; }

private:
    double step_;
    double maxFrame_;
    double accumulator_ = 0.0;
};

} // namespace ee
