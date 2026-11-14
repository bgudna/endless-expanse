#include "app/States.h"

#include "core/Log.h"
#include "render/gl.h"

#include <cmath>
#include <memory>

namespace ee {

namespace {
void clear(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
} // namespace

// ---- Boot ---------------------------------------------------------------
void BootState::update(double dt, StateStack& stack) {
    elapsed_ += dt;
    if (elapsed_ >= 0.5) stack.replace(std::make_unique<MainMenuState>());
}
void BootState::render(double) { clear(0.0f, 0.0f, 0.0f); }

// ---- Main menu ----------------------------------------------------------
void MainMenuState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Left]) stack.replace(std::make_unique<PlayingState>());
}
void MainMenuState::render(double) { clear(0.02f, 0.02f, 0.10f); }

// ---- Playing ------------------------------------------------------------
void PlayingState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Right]) stack.push(std::make_unique<PausedState>());
}
void PlayingState::update(double dt, StateStack&) {
    prevSimTime_ = simTime_;
    simTime_ += dt;
}
void PlayingState::render(double alpha) {
    // Interpolated time, to exercise the render-interpolation path.
    double t = prevSimTime_ + (simTime_ - prevSimTime_) * alpha;
    float pulse = 0.04f + 0.03f * static_cast<float>(std::sin(t * 2.0));
    clear(0.0f, pulse, pulse * 0.5f);
}

// ---- Paused -------------------------------------------------------------
void PausedState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Left]) stack.pop();
    if (input.pressed[InputState::Right]) {
        stack.clear();
        stack.push(std::make_unique<MainMenuState>());
    }
}
void PausedState::render(double) {
    // Intentionally empty: the state beneath is still drawn. A dimming overlay and
    // pause text arrive with the vector renderer (M2).
}

} // namespace ee
