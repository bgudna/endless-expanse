#pragma once

#include "core/InputState.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace ee {

class StateStack;

// A game state (menu, playing, paused...). Must not depend on SDL or GL
// headers at this interface level; concrete states may.
class GameState {
public:
    virtual ~GameState() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    // Called once per rendered frame with fresh input.
    virtual void handleInput(const InputState& input, StateStack& stack) = 0;
    // Called zero or more times per frame at a fixed rate.
    virtual void update(double dt, StateStack& stack) = 0;
    // Called once per frame. alpha in [0,1) interpolates between sim steps.
    virtual void render(double alpha) = 0;

    // If true, states below this one are still rendered (e.g. pause overlay).
    virtual bool isOverlay() const { return false; }
    // If true, states below this one still get update() calls.
    virtual bool updatesBelow() const { return false; }
};

// Stack of states. push/pop/replace requests are deferred and applied by
// applyPending(), so states may safely request changes from their callbacks.
class StateStack {
public:
    void push(std::unique_ptr<GameState> state);
    void pop();
    void replace(std::unique_ptr<GameState> state);
    void clear();

    void applyPending();

    void handleInput(const InputState& input);
    void update(double dt);
    void render(double alpha);

    bool empty() const { return states_.empty(); }
    std::size_t size() const { return states_.size(); }
    GameState* top() { return states_.empty() ? nullptr : states_.back().get(); }

private:
    enum class Op { Push, Pop, Replace, Clear };
    struct Pending {
        Op op;
        std::unique_ptr<GameState> state;
    };

    std::vector<std::unique_ptr<GameState>> states_;
    std::vector<Pending> pending_;
};

} // namespace ee
