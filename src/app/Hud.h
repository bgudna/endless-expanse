#pragma once

#include "core/InputState.h"
#include "render/Renderer.h"

#include <glm/glm.hpp>

#include <string>

namespace ee {

struct Rect {
    glm::vec2 pos{0.0f};
    glm::vec2 size{0.0f};
    bool contains(glm::vec2 p) const {
        return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y && p.y <= pos.y + size.y;
    }
};

// Values the HUD displays (it owns no game state).
struct HudState {
    float zoom = 1.0f;
    float zoomUnit = 1.0f; // 0 = far out, 1 = close in
    BloomQuality bloom = BloomQuality::High;
    int objects = 0; // loaded sector objects (debug readout)
    int sectorsLoaded = 0;
    bool follow = true;
    bool showGrid = true;
    bool dragAssist = true;
    bool weaponsFree = false;
    bool canOrbit = false; // something is selected to orbit
    bool orbiting = false;
    float hull = 1.0f;   // 0..1
    float shield = 1.0f; // 0..1
    std::string status;  // short warning under the bars (nebula, radiation, ...)
    std::string message; // latest event message, fades with messageAlpha
    float messageAlpha = 0.0f;
    bool shipDestroyed = false;
    // Incoming hail (trader / distress call); shown as a panel with two buttons.
    bool hailActive = false;
    std::string hailTitle;
    std::string hailText;
    std::string hailAccept;
    glm::vec2 mouseWorld{0.0f};
    std::string info; // multi-line ship / selection readout (left side)
};

// What the player asked for this frame.
struct HudEvents {
    bool consumedMouse = false; // pointer is over / dragging a HUD element
    bool toggleFollow = false;
    bool toggleAssist = false;
    bool pause = false;
    bool cycleBloom = false;
    bool toggleGrid = false;
    bool toggleWeapons = false;
    bool orbitSelected = false;
    bool hailAccept = false;
    bool hailDecline = false;
    float zoomUnit = -1.0f; // >= 0 when the slider moved
};

// Mouse-driven HUD for the demo scene: a zoom slider and a row of buttons.
// (A reusable UI toolkit arrives in M9.)
class Hud {
public:
    enum Button { Follow, Assist, Pause, Bloom, Grid, Weapons, Orbit, ButtonCount };

    void layout(glm::vec2 screenSize, const HudState& state);
    HudEvents handleInput(const InputState& input);
    void render(Renderer& r, const HudState& state) const;

private:
    Rect buttons_[ButtonCount];
    std::string labels_[ButtonCount];
    bool enabled_[ButtonCount] = {};
    Rect sliderHit_;
    Rect hailPanel_, hailAcceptBtn_, hailDeclineBtn_;
    bool hailShown_ = false;
    float sliderTop_ = 0.0f;
    float sliderBottom_ = 0.0f;
    float sliderX_ = 0.0f;
    glm::vec2 screen_{1280.0f, 720.0f};

    glm::vec2 mouse_{0.0f};
    bool dragging_ = false;
};

} // namespace ee
