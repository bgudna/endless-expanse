#pragma once

#include "core/FixedTimestep.h"
#include "core/InputState.h"
#include "core/StateStack.h"
#include "platform/Input.h"
#include "platform/Window.h"
#include "render/Renderer.h"

#include <string>

namespace ee {

class Application {
public:
    bool init();
    int run();

private:
    void updateStats(double frameSeconds, int steps);
    void drawDebugOverlay();

    Window window_;
    Renderer renderer_;
    Input input_;
    InputState inputState_;
    StateStack states_;
    FixedTimestep timestep_{1.0 / 60.0};

    // Toggled by a middle-click (not a drag).
    bool debugOverlay_ = true;
    float middleTravel_ = 0.0f;
    std::string debugText_;
    double statAccum_ = 0.0;
    int statFrames_ = 0;
    int statSteps_ = 0;
    double statWorstFrame_ = 0.0;
};

} // namespace ee
