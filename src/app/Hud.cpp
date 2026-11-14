#include "app/Hud.h"

#include "camera/Camera2D.h"
#include "vector/StrokeFont.h"

#include <algorithm>
#include <cstdio>

namespace ee {

namespace {
constexpr float kButtonHeight = 26.0f;
constexpr float kButtonTextHeight = 11.0f;
constexpr float kButtonPadding = 12.0f;
constexpr float kButtonGap = 8.0f;
constexpr float kMargin = 16.0f;

void drawRect(LineBatch& lines, const Rect& r, const glm::vec4& color, float width) {
    const glm::vec2 a = r.pos;
    const glm::vec2 b = r.pos + glm::vec2(r.size.x, 0.0f);
    const glm::vec2 c = r.pos + r.size;
    const glm::vec2 d = r.pos + glm::vec2(0.0f, r.size.y);
    lines.line(a, b, color, width);
    lines.line(b, c, color, width);
    lines.line(c, d, color, width);
    lines.line(d, a, color, width);
}
} // namespace

void Hud::layout(glm::vec2 screenSize, const HudState& state) {
    screen_ = screenSize;

    labels_[Recenter] = "RECENTER";
    labels_[Pause] = "PAUSE";
    labels_[Bloom] = std::string("BLOOM ") + toString(state.bloom);
    labels_[RocksMinus] = "ROCKS -";
    labels_[RocksPlus] = "ROCKS +";

    float x = kMargin;
    const float y = screenSize.y - kMargin - kButtonHeight;
    for (int i = 0; i < ButtonCount; ++i) {
        const float w =
            StrokeFont::measure(labels_[i], kButtonTextHeight).x + 2.0f * kButtonPadding;
        buttons_[i] = {{x, y}, {w, kButtonHeight}};
        x += w + kButtonGap;
    }

    sliderX_ = screenSize.x - 36.0f;
    sliderTop_ = 130.0f;
    sliderBottom_ = std::max(sliderTop_ + 60.0f, screenSize.y - 130.0f);
    sliderHit_ = {{sliderX_ - 16.0f, sliderTop_ - 10.0f},
                  {32.0f, sliderBottom_ - sliderTop_ + 20.0f}};
}

HudEvents Hud::handleInput(const InputState& input) {
    HudEvents ev;
    mouse_ = {input.mouseX, input.mouseY};

    bool overButton = false;
    for (int i = 0; i < ButtonCount; ++i) {
        if (!buttons_[i].contains(mouse_)) continue;
        overButton = true;
        if (!input.pressed[InputState::Left]) continue;
        switch (i) {
        case Recenter: ev.recenter = true; break;
        case Pause: ev.pause = true; break;
        case Bloom: ev.cycleBloom = true; break;
        case RocksMinus: ev.rockDelta = -1; break;
        case RocksPlus: ev.rockDelta = 1; break;
        default: break;
        }
    }

    if (input.pressed[InputState::Left] && sliderHit_.contains(mouse_)) dragging_ = true;
    if (!input.down[InputState::Left]) dragging_ = false;
    if (dragging_) {
        const float t = (sliderBottom_ - mouse_.y) / (sliderBottom_ - sliderTop_);
        ev.zoomUnit = std::clamp(t, 0.0f, 1.0f);
    }

    ev.consumedMouse = overButton || dragging_ || sliderHit_.contains(mouse_);
    return ev;
}

void Hud::render(Renderer& r, const HudState& state) const {
    LineBatch& lines = r.lines();
    lines.setView(r.screenView());

    // Buttons.
    for (int i = 0; i < ButtonCount; ++i) {
        const bool hover = buttons_[i].contains(mouse_);
        const glm::vec4 color = hover ? colors::amber : colors::scaled(colors::phosphor, 0.7f);
        drawRect(lines, buttons_[i], color, 1.5f);
        r.drawTextCentered(labels_[i], buttons_[i].pos + buttons_[i].size * 0.5f,
                           kButtonTextHeight, color, 1.3f);
    }

    // Zoom slider: top = zoomed in, bottom = zoomed out.
    const glm::vec4 track = colors::scaled(colors::cyan, 0.5f);
    lines.line({sliderX_, sliderTop_}, {sliderX_, sliderBottom_}, track, 1.5f);
    for (int i = 0; i <= 4; ++i) {
        const float y = sliderBottom_ - (sliderBottom_ - sliderTop_) * static_cast<float>(i) / 4.0f;
        lines.line({sliderX_ - 5.0f, y}, {sliderX_ + 5.0f, y}, track, 1.2f);
    }
    r.drawTextCentered("+", {sliderX_, sliderTop_ - 22.0f}, 12.0f, colors::cyan, 1.4f);
    r.drawTextCentered("-", {sliderX_, sliderBottom_ + 22.0f}, 12.0f, colors::cyan, 1.4f);

    const float handleY = sliderBottom_ - (sliderBottom_ - sliderTop_) * state.zoomUnit;
    const bool active = dragging_ || sliderHit_.contains(mouse_);
    drawRect(lines, {{sliderX_ - 10.0f, handleY - 5.0f}, {20.0f, 10.0f}},
             active ? colors::amber : colors::phosphor, 1.8f);

    // Readouts.
    char buf[64];
    std::snprintf(buf, sizeof buf, "ZOOM %.2fX", static_cast<double>(state.zoom));
    r.drawText(buf, {screen_.x - 16.0f - StrokeFont::measure(buf, 11.0f).x, 96.0f}, 11.0f,
               colors::cyan, 1.2f);
    std::snprintf(buf, sizeof buf, "X %.0f  Y %.0f", static_cast<double>(state.mouseWorld.x),
                  static_cast<double>(state.mouseWorld.y));
    r.drawText(buf, {screen_.x - 16.0f - StrokeFont::measure(buf, 11.0f).x, 74.0f}, 11.0f,
               colors::scaled(colors::cyan, 0.7f), 1.2f);

    std::snprintf(buf, sizeof buf, "ROCKS %d", state.rocks);
    r.drawText(buf, {kMargin, screen_.y - kMargin - kButtonHeight - 22.0f}, 11.0f,
               colors::scaled(colors::amber, 0.8f), 1.2f);

    r.drawTextCentered("WHEEL: ZOOM   DRAG: PAN", {screen_.x * 0.5f, 24.0f}, 11.0f,
                       colors::scaled(colors::white, 0.5f), 1.2f);
}

} // namespace ee
