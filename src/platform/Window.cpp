#include "platform/Window.h"

#include "core/Log.h"
#include "render/gl.h"

#include <SDL.h>

namespace ee {

Window::~Window() {
    if (glContext_) SDL_GL_DeleteContext(static_cast<SDL_GLContext>(glContext_));
    if (window_) SDL_DestroyWindow(window_);
}

bool Window::create(const std::string& title, int width, int height) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                               height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        EE_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        EE_LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    vsync_ = (SDL_GL_SetSwapInterval(1) == 0);
    if (!vsync_) EE_LOG_WARN("VSync unavailable: %s", SDL_GetError());

    EE_LOG_INFO("GL_VERSION: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    EE_LOG_INFO("GL_RENDERER: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    return true;
}

void Window::swap() { SDL_GL_SwapWindow(window_); }

void Window::setTitle(const std::string& title) { SDL_SetWindowTitle(window_, title.c_str()); }

void Window::drawableSize(int& w, int& h) const { SDL_GL_GetDrawableSize(window_, &w, &h); }

float Window::pixelRatio() const {
    int dw = 0, dh = 0, lw = 0, lh = 0;
    SDL_GL_GetDrawableSize(window_, &dw, &dh);
    SDL_GetWindowSize(window_, &lw, &lh);
    return lw > 0 ? static_cast<float>(dw) / static_cast<float>(lw) : 1.0f;
}

} // namespace ee
