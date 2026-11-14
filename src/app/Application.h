#pragma once

#include "core/FixedTimestep.h"
#include "core/InputState.h"
#include "core/StateStack.h"
#include "platform/Input.h"
#include "platform/Window.h"

namespace ee {

class Application {
public:
    bool init();
    int run();

    // Toggled by middle-click: shows FPS / frame time / sim steps in the window title.
    bool debugOverlay() const { return debugOverlay_; }

private:
    void updateTitleStats(double frameSeconds, int steps);

    Window window_;
    Input input_;
    InputState inputState_;
    StateStack states_;
    FixedTimestep timestep_{1.0 / 60.0};

    bool debugOverlay_ = true;
    double statAccum_ = 0.0;
    int statFrames_ = 0;
    int statSteps_ = 0;
    double statWorstFrame_ = 0.0;
};

} // namespace ee
