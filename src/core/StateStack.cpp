#include "core/StateStack.h"

namespace ee {

void StateStack::push(std::unique_ptr<GameState> state) {
    pending_.push_back({Op::Push, std::move(state)});
}
void StateStack::pop() { pending_.push_back({Op::Pop, nullptr}); }
void StateStack::replace(std::unique_ptr<GameState> state) {
    pending_.push_back({Op::Replace, std::move(state)});
}
void StateStack::clear() { pending_.push_back({Op::Clear, nullptr}); }

void StateStack::applyPending() {
    // Index loop: callbacks (onEnter/onExit) may queue more requests.
    for (size_t i = 0; i < pending_.size(); ++i) {
        Pending p = std::move(pending_[i]);
        switch (p.op) {
        case Op::Push:
            states_.push_back(std::move(p.state));
            states_.back()->onEnter();
            break;
        case Op::Pop:
            if (!states_.empty()) {
                states_.back()->onExit();
                states_.pop_back();
            }
            break;
        case Op::Replace:
            if (!states_.empty()) {
                states_.back()->onExit();
                states_.pop_back();
            }
            states_.push_back(std::move(p.state));
            states_.back()->onEnter();
            break;
        case Op::Clear:
            while (!states_.empty()) {
                states_.back()->onExit();
                states_.pop_back();
            }
            break;
        }
    }
    pending_.clear();
}

void StateStack::handleInput(const InputState& input) {
    if (!states_.empty()) states_.back()->handleInput(input, *this);
}

void StateStack::update(double dt) {
    for (size_t i = states_.size(); i-- > 0;) {
        states_[i]->update(dt, *this);
        if (!states_[i]->updatesBelow()) break;
    }
}

void StateStack::render(double alpha) {
    size_t first = states_.size();
    while (first > 0) {
        --first;
        if (!states_[first]->isOverlay()) break;
    }
    for (size_t i = first; i < states_.size(); ++i) states_[i]->render(alpha);
}

} // namespace ee
