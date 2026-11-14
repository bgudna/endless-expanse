#include "app/Application.h"

#include "app/States.h"
#include "core/Log.h"
#include "render/gl.h"

#include <SDL.h>

#include <cstdio>

namespace ee {

bool Application::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        EE_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!window_.create("Endless Expanse", 1280, 720)) return false;

    states_.push(std::make_unique<BootState>());
    states_.applyPending();
    return true;
}

void Application::updateTitleStats(double frameSeconds, int steps) {
    statAccum_ += frameSeconds;
    ++statFrames_;
    statSteps_ += steps;
    if (frameSeconds > statWorstFrame_) statWorstFrame_ = frameSeconds;
    if (statAccum_ < 0.5) return;

    if (debugOverlay_) {
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "Endless Expanse | %.0f fps | %.2f ms avg, %.2f ms worst | %d sim steps/s%s",
                      statFrames_ / statAccum_, statAccum_ / statFrames_ * 1000.0,
                      statWorstFrame_ * 1000.0, static_cast<int>(statSteps_ / statAccum_ + 0.5),
                      window_.vsyncEnabled() ? "" : " | no vsync");
        window_.setTitle(buf);
    }
    statAccum_ = 0.0;
    statFrames_ = 0;
    statSteps_ = 0;
    statWorstFrame_ = 0.0;
}

int Application::run() {
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 last = SDL_GetPerformanceCounter();

    while (!inputState_.quitRequested && !states_.empty()) {
        Uint64 now = SDL_GetPerformanceCounter();
        double frameSeconds = static_cast<double>(now - last) / freq;
        last = now;

        input_.poll(inputState_);
        if (inputState_.pressed[InputState::Middle]) {
            debugOverlay_ = !debugOverlay_;
            if (!debugOverlay_) window_.setTitle("Endless Expanse");
        }

        states_.handleInput(inputState_);
        states_.applyPending();

        int steps = timestep_.advance(frameSeconds);
        for (int i = 0; i < steps; ++i) {
            states_.update(timestep_.step());
            states_.applyPending();
        }

        int w = 0, h = 0;
        window_.drawableSize(w, h);
        glViewport(0, 0, w, h);
        states_.render(timestep_.alpha());
        window_.swap();

        updateTitleStats(frameSeconds, steps);

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
