#pragma once

#include "app/Hud.h"
#include "camera/Camera2D.h"
#include "core/Random.h"
#include "core/StateStack.h"
#include "render/Renderer.h"
#include "sim/ExhaustTrail.h"
#include "sim/Ship.h"
#include "vector/VectorShape.h"
#include "world/Galaxy.h"
#include "world/SectorStreamer.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
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

// Playing: an endless procedurally generated galaxy streamed in sectors.
//   wheel: zoom   left-drag / middle-drag: pan   right-click (or hold): fly there
//   left-click: select   double-click: fly to the selected object
class PlayingState : public GameState {
public:
    explicit PlayingState(Renderer& r, std::uint64_t galaxySeed = 20260101);
    void handleInput(const InputState& input, StateStack& stack) override;
    void update(double dt, StateStack& stack) override;
    void render(double alpha) override;

private:
    struct Star {
        glm::vec2 unitPos; // position within the screen, 0..1
        float depth;       // parallax factor
        float brightness;
    };
    // Identified by absolute sector coordinates, so it survives origin shifts.
    struct Selection {
        bool valid = false;
        SectorCoord sector;
        std::size_t index = 0;
    };

    HudState hudState() const;
    void drawGrid(const CameraView& v);
    void drawSectorBorders(const CameraView& v);
    void drawSectorLabels(const CameraView& v);
    void drawStars(const CameraView& v);
    void drawDestination(const CameraView& v, double t);
    void drawSector(const CameraView& v, const Sector& sector, double t);
    void drawObject(const CameraView& v, const SectorObject& o, glm::vec2 offset, double t);

    void visibleRange(const CameraView& v, SectorCoord& lo, SectorCoord& hi) const;
    void rebaseOrigin();
    glm::dvec2 absolutePosition(glm::vec2 local) const;

    Selection pick(const CameraView& v, glm::vec2 screenPoint) const;
    const SectorObject* selectedObject() const;
    glm::vec2 selectionPos() const;
    std::string selectionName() const;
    void flyToSelection();

    Renderer& r_;
    Camera2D camera_;
    Hud hud_;
    Ship ship_;
    ExhaustTrail exhaust_;
    Random rng_{777};
    Galaxy galaxy_;
    SectorStreamer streamer_;
    SectorCoord origin_; // the sector whose centre is world (0, 0)
    VectorShape shipShape_, planetShape_, circleShape_, orbitShape_, diamondShape_, starCrossShape_;
    std::vector<VectorShape> rockShapes_;
    std::vector<Star> stars_;
    bool follow_ = true;
    bool showGrid_ = true;
    Selection selection_;
    double lastClickTime_ = -10.0;
    glm::vec2 lastClickPos_{0.0f};
    glm::vec2 mouse_{0.0f};
    bool leftOnWorld_ = false;
    bool middleOnWorld_ = false;
    bool rightOnWorld_ = false;
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
