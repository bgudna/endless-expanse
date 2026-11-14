#pragma once

namespace ee {

// Plain-data snapshot of mouse input for one rendered frame.
// Edge flags (pressed/released/wheel/delta) are valid for this frame only.
struct InputState {
    enum Button { Left = 0, Middle = 1, Right = 2, ButtonCount = 3 };

    float mouseX = 0.0f; // window coordinates (logical points)
    float mouseY = 0.0f;
    float deltaX = 0.0f; // movement since last frame
    float deltaY = 0.0f;
    float wheelY = 0.0f; // positive = scroll up / zoom in

    bool down[ButtonCount] = {};
    bool pressed[ButtonCount] = {};
    bool released[ButtonCount] = {};

    bool quitRequested = false;
    bool windowResized = false;
};

} // namespace ee
