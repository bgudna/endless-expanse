#include "platform/Input.h"

#include <SDL.h>

namespace ee {

namespace {
int toButton(Uint8 sdlButton) {
    switch (sdlButton) {
    case SDL_BUTTON_LEFT: return InputState::Left;
    case SDL_BUTTON_MIDDLE: return InputState::Middle;
    case SDL_BUTTON_RIGHT: return InputState::Right;
    default: return -1;
    }
}
} // namespace

void Input::poll(InputState& out) {
    // Reset per-frame edges; keep persistent state.
    for (int i = 0; i < InputState::ButtonCount; ++i) {
        out.pressed[i] = false;
        out.released[i] = false;
    }
    out.wheelY = 0.0f;
    out.windowResized = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: out.quitRequested = true; break;
        case SDL_MOUSEMOTION:
            out.mouseX = static_cast<float>(e.motion.x);
            out.mouseY = static_cast<float>(e.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN: {
            int b = toButton(e.button.button);
            if (b >= 0) {
                down_[b] = true;
                out.pressed[b] = true;
            }
            out.mouseX = static_cast<float>(e.button.x);
            out.mouseY = static_cast<float>(e.button.y);
            break;
        }
        case SDL_MOUSEBUTTONUP: {
            int b = toButton(e.button.button);
            if (b >= 0) {
                down_[b] = false;
                out.released[b] = true;
            }
            break;
        }
        case SDL_MOUSEWHEEL: out.wheelY += e.wheel.preciseY; break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                e.window.event == SDL_WINDOWEVENT_RESIZED)
                out.windowResized = true;
            break;
        default: break;
        }
    }

    for (int i = 0; i < InputState::ButtonCount; ++i) out.down[i] = down_[i];

    if (!haveLast_) {
        lastX_ = out.mouseX;
        lastY_ = out.mouseY;
        haveLast_ = true;
    }
    out.deltaX = out.mouseX - lastX_;
    out.deltaY = out.mouseY - lastY_;
    lastX_ = out.mouseX;
    lastY_ = out.mouseY;
}

} // namespace ee
