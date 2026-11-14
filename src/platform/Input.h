#pragma once

#include "core/InputState.h"

namespace ee {

// Polls SDL events and fills an InputState for the frame.
class Input {
public:
    void poll(InputState& out);

private:
    bool down_[InputState::ButtonCount] = {};
    float lastX_ = 0.0f;
    float lastY_ = 0.0f;
    bool haveLast_ = false;
};

} // namespace ee
