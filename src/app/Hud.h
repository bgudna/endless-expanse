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
    float zoomUnit = -1.0f; // >= 0 when the slider moved
};

// Mouse-driven HUD for the demo scene: a zoom slider and a row of buttons.
// (A reusable UI toolkit arrives in M9.)
class Hud {
public:
    enum Button { Follow, Assist, Pause, Bloom, Grid, ButtonCount };

    void layout(glm::vec2 screenSize, const HudState& state);
    HudEvents handleInput(const InputState& input);
    void render(Renderer& r, const HudState& state) const;

private:
    Rect buttons_[ButtonCount];
    std::string labels_[ButtonCount];
    Rect sliderHit_;
    float sliderTop_ = 0.0f;
    float sliderBottom_ = 0.0f;
    float sliderX_ = 0.0f;
    glm::vec2 screen_{1280.0f, 720.0f};

    glm::vec2 mouse_{0.0f};
    bool dragging_ = false;
};

} // namespace ee
