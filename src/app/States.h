#pragma once

#include "app/Hud.h"
#include "camera/Camera2D.h"
#include "core/Random.h"
#include "core/StateStack.h"
#include "render/Renderer.h"
#include "sim/ExhaustTrail.h"
#include "sim/Ship.h"
#include "sim/Simulation.h"
#include "vector/VectorShape.h"
#include "world/Galaxy.h"
#include "world/SectorStreamer.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
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

// Playing: an endless procedurally generated galaxy with hazards, aliens and combat.
//   wheel: zoom   left-drag / middle-drag: pan   right-click (or hold): fly there
//   left-click: select / target   double-click or ORBIT: orbit the selected object
// Orbits follow moving ships; the orbit is tied to its target, not to the current selection.
// All game rules live in the headless Simulation; this class is input, camera and drawing.
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
    // A world object is identified by absolute sector coordinates (survives origin shifts);
    // an alien by its id.
    struct Selection {
        enum class Kind { None, Object, Alien } kind = Kind::None;
        SectorCoord sector;
        std::size_t index = 0;
        int alienId = -1;
    };
    // Short-lived explosion / impact effect (cosmetic only).
    struct Burst {
        glm::vec2 pos;
        float age;
        float life;
        float size;
    };
    // Where a selection is right now, or nullopt when it has been destroyed or unloaded.
    struct Located {
        glm::vec2 pos;
        float radius;
        bool nebula;
        glm::vec2 velocity;
    };

    HudState hudState() const;
    void drawGrid(const CameraView& v);
    void drawSectorBorders(const CameraView& v);
    void drawSectorLabels(const CameraView& v);
    void drawStars(const CameraView& v);
    void drawDestination(const CameraView& v, double t);
    void drawSector(const CameraView& v, const Sector& sector, double t);
    void drawObject(const CameraView& v, const SectorObject& o, glm::vec2 offset, double t);
    void drawAliens(const CameraView& v, double alpha, double t);
    void drawProjectilesAndBursts(const CameraView& v, double alpha);
    void drawShieldFlash(const CameraView& v, glm::vec2 pos, float radius, const Vitals& vit);

    void visibleRange(const CameraView& v, SectorCoord& lo, SectorCoord& hi) const;
    void processEvents();

    Selection pick(const CameraView& v, glm::vec2 screenPoint) const;
    const SectorObject* objectAt(const Selection& sel) const;
    const AlienShip* alienAt(const Selection& sel) const;
    std::optional<Located> locate(const Selection& sel) const;
    bool hasSelection() const { return locate(selection_).has_value(); }
    std::string selectionName(const Selection& sel) const;
    void startOrbit();
    void updateOrbit();

    Renderer& r_;
    std::uint64_t seed_;
    Camera2D camera_;
    Hud hud_;
    Simulation sim_;
    ExhaustTrail exhaust_;
    Random rng_{777};
    VectorShape shipShape_, planetShape_, circleShape_, orbitShape_, diamondShape_, starCrossShape_,
        mineShape_, holeShape_;
    std::vector<VectorShape> rockShapes_;
    std::vector<VectorShape> alienShapes_;
    std::vector<Star> stars_;
    std::vector<Burst> bursts_;
    std::string message_;
    float messageAge_ = 99.0f;
    bool follow_ = true;
    bool showGrid_ = true;
    Selection selection_;
    Selection orbitSel_;
    bool orbiting_ = false;
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
