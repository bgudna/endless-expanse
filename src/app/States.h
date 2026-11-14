#pragma once

#include "app/Hud.h"
#include "camera/Camera2D.h"
#include "core/StateStack.h"
#include "render/Renderer.h"
#include "vector/VectorShape.h"

#include <glm/glm.hpp>

#include <vector>

namespace ee {

// Brief startup state; hands off to the main menu.
class BootState : public GameState {
public:
    explicit BootState(Renderer& r) : r_(r) {}
    void handleInput(const InputState&, StateStack&) override {}
    void update(double dt, StateStack& stack) override;
    void render(double alpha) override;

private:
    Renderer& r_;
    double elapsed_ = 0.0;
};

// Click anywhere to start playing.
class MainMenuState : public GameState {
public:
    explicit MainMenuState(Renderer& r);
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double dt, StateStack&) override;
    void render(double alpha) override;

private:
    Renderer& r_;
    VectorShape ship_;
    double time_ = 0.0;
    double prevTime_ = 0.0;
};

// M3 demo scene: a large sector with a pan/zoom camera.
//   wheel: zoom at cursor   left/middle drag: pan   HUD buttons for everything else
class PlayingState : public GameState {
public:
    explicit PlayingState(Renderer& r);
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double dt, StateStack& stack) override;
    void render(double alpha) override;

private:
    struct Rock {
        glm::vec2 pos;
        float radius;
        float spin;
        int variant;
    };
    struct Planet {
        glm::vec2 pos;
        float radius;
        glm::vec4 color;
    };
    struct Star {
        glm::vec2 unitPos; // position within the screen, 0..1
        float depth;       // parallax factor
        float brightness;
    };

    HudState hudState() const;
    void drawGrid(const CameraView& v);
    void drawStars(const CameraView& v);

    Renderer& r_;
    Camera2D camera_;
    Hud hud_;
    VectorShape ship_, planet_;
    std::vector<VectorShape> rockShapes_;
    std::vector<Rock> rocks_;
    std::vector<Planet> planets_;
    std::vector<Star> stars_;
    int rockCount_ = 300;
    glm::vec2 shipPos_{0.0f};
    glm::vec2 mouse_{0.0f};
    bool leftOnWorld_ = false;
    bool middleOnWorld_ = false;
    float leftTravel_ = 0.0f;
    double simTime_ = 0.0;
    double prevSimTime_ = 0.0;
};

// Overlay: left-click resumes, right-click returns to the main menu.
class PausedState : public GameState {
public:
    explicit PausedState(Renderer& r) : r_(r) {}
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double, StateStack&) override {}
    void render(double alpha) override;
    bool isOverlay() const override { return true; }

private:
    Renderer& r_;
};

} // namespace ee
