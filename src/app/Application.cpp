#include "app/Application.h"

#include "app/States.h"
#include "core/Log.h"
#include "render/gl.h"

#include <SDL.h>

#include <cmath>
#include <cstdio>

namespace ee {

bool Application::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        EE_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!window_.create("Endless Expanse", 1280, 720)) return false;
    if (!renderer_.init()) return false;

    states_.push(std::make_unique<BootState>(renderer_));
    states_.applyPending();
    return true;
}

void Application::updateStats(double frameSeconds, int steps) {
    statAccum_ += frameSeconds;
    ++statFrames_;
    statSteps_ += steps;
    if (frameSeconds > statWorstFrame_) statWorstFrame_ = frameSeconds;
    if (statAccum_ < 0.5) return;

    const LineBatch::Stats& ls = renderer_.lines().lastFrameStats();
    char buf[200];
    std::snprintf(buf, sizeof buf,
                  "%.0f FPS  %.2f MS (WORST %.1f)\nSIM %d/S  SEGS %zu  DRAWS %zu\nBLOOM %s%s",
                  statFrames_ / statAccum_, statAccum_ / statFrames_ * 1000.0,
                  statWorstFrame_ * 1000.0, static_cast<int>(statSteps_ / statAccum_ + 0.5),
                  ls.segments, ls.drawCalls, toString(renderer_.bloomQuality()),
                  window_.vsyncEnabled() ? "" : "  NO VSYNC");
    debugText_ = buf;

    statAccum_ = 0.0;
    statFrames_ = 0;
    statSteps_ = 0;
    statWorstFrame_ = 0.0;
}

void Application::drawDebugOverlay() {
    if (!debugOverlay_ || debugText_.empty()) return;
    renderer_.drawText(debugText_, {16.0f, 16.0f}, 11.0f, colors::scaled(colors::phosphor, 0.8f),
                       1.2f);
}

int Application::run() {
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 last = SDL_GetPerformanceCounter();

    while (!inputState_.quitRequested && !states_.empty()) {
        Uint64 now = SDL_GetPerformanceCounter();
        double frameSeconds = static_cast<double>(now - last) / freq;
        last = now;

        input_.poll(inputState_);
        // Middle-click (without dragging) toggles the debug readout; dragging pans the camera.
        if (inputState_.pressed[InputState::Middle]) middleTravel_ = 0.0f;
        if (inputState_.down[InputState::Middle])
            middleTravel_ += std::hypot(inputState_.deltaX, inputState_.deltaY);
        if (inputState_.released[InputState::Middle] && middleTravel_ < 4.0f)
            debugOverlay_ = !debugOverlay_;

        states_.handleInput(inputState_);
        states_.applyPending();

        int steps = timestep_.advance(frameSeconds);
        for (int i = 0; i < steps; ++i) {
            states_.update(timestep_.step());
            states_.applyPending();
        }

        int w = 0, h = 0;
        window_.drawableSize(w, h);
        renderer_.beginFrame(w, h, window_.pixelRatio());
        states_.render(timestep_.alpha());
        drawDebugOverlay();
        renderer_.endFrame();
        window_.swap();

        updateStats(frameSeconds, steps);

        // Without vsync, cap at ~60 fps instead of spinning the CPU.
        if (!window_.vsyncEnabled()) {
            double spent = static_cast<double>(SDL_GetPerformanceCounter() - now) / freq;
            double remaining = timestep_.step() - spent;
            if (remaining > 0.001) SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
        }
    }

    states_.clear();
    states_.applyPending();
    return 0;
}

} // namespace ee
