#include "core/FixedTimestep.h"

namespace ee {

int FixedTimestep::advance(double frameSeconds) {
    if (frameSeconds < 0.0) frameSeconds = 0.0;
    if (frameSeconds > maxFrame_) frameSeconds = maxFrame_;
    accumulator_ += frameSeconds;
    int steps = 0;
    while (accumulator_ >= step_) {
        accumulator_ -= step_;
        ++steps;
    }
    return steps;
}

} // namespace ee
