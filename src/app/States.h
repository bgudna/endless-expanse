#pragma once

#include "core/StateStack.h"

namespace ee {

// Brief startup state; hands off to the main menu.
class BootState : public GameState {
public:
    void handleInput(const InputState&, StateStack&) override {}
    void update(double dt, StateStack& stack) override;
    void render(double alpha) override;

private:
    double elapsed_ = 0.0;
};

// Click anywhere to start playing.
class MainMenuState : public GameState {
public:
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double, StateStack&) override {}
    void render(double alpha) override;
};

// Placeholder gameplay: right-click pauses.
class PlayingState : public GameState {
public:
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double dt, StateStack& stack) override;
    void render(double alpha) override;

private:
    double simTime_ = 0.0;
    double prevSimTime_ = 0.0;
};

// Overlay: left-click resumes, right-click returns to the main menu.
class PausedState : public GameState {
public:
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double, StateStack&) override {}
    void render(double alpha) override;
    bool isOverlay() const override { return true; }
};

} // namespace ee
