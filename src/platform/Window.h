#pragma once

#include <string>

struct SDL_Window;

namespace ee {

// Owns the SDL window and OpenGL 3.3 core context.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const std::string& title, int width, int height);
    void swap();
    void setTitle(const std::string& title);

    // Drawable size in pixels (differs from window size on HiDPI displays).
    void drawableSize(int& w, int& h) const;
    // Drawable pixels per logical point (2.0 on Retina displays).
    float pixelRatio() const;
    bool vsyncEnabled() const { return vsync_; }

private:
    SDL_Window* window_ = nullptr;
    void* glContext_ = nullptr;
    bool vsync_ = false;
};

} // namespace ee
