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

    labels_[Follow] = state.follow ? "FOLLOW ON" : "FOLLOW OFF";
    labels_[Assist] = state.dragAssist ? "ASSIST ON" : "ASSIST OFF";
    labels_[Pause] = "PAUSE";
    labels_[Bloom] = std::string("BLOOM ") + toString(state.bloom);
    labels_[Grid] = state.showGrid ? "GRID ON" : "GRID OFF";
    labels_[Weapons] = state.weaponsFree ? "FIRE: FREE" : "FIRE: HOSTILE ONLY";
    labels_[Orbit] = state.orbiting ? "ORBITING" : "ORBIT";

    float x = kMargin;
    const float y = screenSize.y - kMargin - kButtonHeight;
    for (int i = 0; i < ButtonCount; ++i) {
        enabled_[i] = i != Orbit || state.canOrbit;
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

    hailShown_ = state.hailActive;
    const glm::vec2 panelSize(520.0f, 120.0f);
    hailPanel_ = {{screenSize.x * 0.5f - panelSize.x * 0.5f, 90.0f}, panelSize};
    const glm::vec2 btn(150.0f, kButtonHeight);
    const float by = hailPanel_.pos.y + panelSize.y - btn.y - 14.0f;
    hailAcceptBtn_ = {{hailPanel_.pos.x + panelSize.x * 0.5f - btn.x - 10.0f, by}, btn};
    hailDeclineBtn_ = {{hailPanel_.pos.x + panelSize.x * 0.5f + 10.0f, by}, btn};
}

HudEvents Hud::handleInput(const InputState& input) {
    HudEvents ev;
    mouse_ = {input.mouseX, input.mouseY};

    bool overButton = false;
    for (int i = 0; i < ButtonCount; ++i) {
        if (!buttons_[i].contains(mouse_)) continue;
        overButton = true; // disabled buttons still swallow the click
        if (!enabled_[i] || !input.pressed[InputState::Left]) continue;
        switch (i) {
        case Follow: ev.toggleFollow = true; break;
        case Assist: ev.toggleAssist = true; break;
        case Pause: ev.pause = true; break;
        case Bloom: ev.cycleBloom = true; break;
        case Grid: ev.toggleGrid = true; break;
        case Weapons: ev.toggleWeapons = true; break;
        case Orbit: ev.orbitSelected = true; break;
        default: break;
        }
    }

    bool overPanel = false;
    if (hailShown_) {
        overPanel = hailPanel_.contains(mouse_);
        if (input.pressed[InputState::Left]) {
            if (hailAcceptBtn_.contains(mouse_)) ev.hailAccept = true;
            if (hailDeclineBtn_.contains(mouse_)) ev.hailDecline = true;
        }
    }

    if (input.pressed[InputState::Left] && sliderHit_.contains(mouse_)) dragging_ = true;
    if (!input.down[InputState::Left]) dragging_ = false;
    if (dragging_) {
        const float t = (sliderBottom_ - mouse_.y) / (sliderBottom_ - sliderTop_);
        ev.zoomUnit = std::clamp(t, 0.0f, 1.0f);
    }

    ev.consumedMouse = overButton || overPanel || dragging_ || sliderHit_.contains(mouse_);
    return ev;
}

void Hud::render(Renderer& r, const HudState& state) const {
    LineBatch& lines = r.lines();
    lines.setView(r.screenView());

    // Buttons.
    for (int i = 0; i < ButtonCount; ++i) {
        const bool hover = enabled_[i] && buttons_[i].contains(mouse_);
        const glm::vec4 color = !enabled_[i] ? colors::scaled(colors::phosphor, 0.3f)
                                : hover      ? colors::amber
                                             : colors::scaled(colors::phosphor, 0.7f);
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

    std::snprintf(buf, sizeof buf, "SECTORS %d  OBJECTS %d", state.sectorsLoaded, state.objects);
    r.drawText(buf, {kMargin, screen_.y - kMargin - kButtonHeight - 22.0f}, 11.0f,
               colors::scaled(colors::amber, 0.8f), 1.2f);

    r.drawTextCentered("WHEEL: ZOOM   LEFT-DRAG: PAN   RIGHT-CLICK: GO   CLICK: SELECT   DOUBLE-CLICK: FLY TO",
                       {screen_.x * 0.5f, 26.0f}, 11.0f, colors::scaled(colors::white, 0.5f), 1.2f);

    if (!state.info.empty())
        r.drawText(state.info, {kMargin, 84.0f}, 11.0f, colors::scaled(colors::phosphor, 0.9f), 1.2f);

    // Hull and shield bars (top centre).
    {
        const float w = 220.0f, h = 9.0f, x = screen_.x * 0.5f - w * 0.5f + 20.0f;
        auto bar = [&](float y, const char* label, float frac, const glm::vec4& color) {
            r.drawText(label, {x - 66.0f, y - 1.0f}, 10.0f, color, 1.1f);
            drawRect(lines, {{x, y}, {w, h}}, colors::scaled(color, 0.6f), 1.2f);
            const float fill = std::clamp(frac, 0.0f, 1.0f) * (w - 4.0f);
            if (fill > 0.5f)
                lines.line({x + 2.0f, y + h * 0.5f}, {x + 2.0f + fill, y + h * 0.5f}, color, h - 3.0f);
        };
        const bool low = state.hull < 0.3f;
        bar(46.0f, "HULL", state.hull, low ? colors::red : colors::phosphor);
        bar(62.0f, "SHIELD", state.shield, colors::cyan);
        if (!state.status.empty())
            r.drawTextCentered(state.status, {screen_.x * 0.5f, 86.0f}, 11.0f, colors::amber, 1.2f);
    }

    if (state.messageAlpha > 0.01f && !state.message.empty())
        r.drawTextCentered(state.message, {screen_.x * 0.5f, screen_.y - 70.0f}, 13.0f,
                           colors::scaled(colors::white, state.messageAlpha), 1.3f);

    if (hailShown_) {
        drawRect(lines, hailPanel_, colors::amber, 1.8f);
        r.drawTextCentered(state.hailTitle, hailPanel_.pos + glm::vec2(hailPanel_.size.x * 0.5f, 22.0f), 13.0f,
                           colors::amber, 1.4f);
        r.drawTextCentered(state.hailText, hailPanel_.pos + glm::vec2(hailPanel_.size.x * 0.5f, 48.0f), 10.0f,
                           colors::white, 1.1f);
        const Rect* btns[2] = {&hailAcceptBtn_, &hailDeclineBtn_};
        const std::string labels[2] = {state.hailAccept, "DECLINE"};
        for (int i = 0; i < 2; ++i) {
            const glm::vec4 c = btns[i]->contains(mouse_) ? colors::amber : colors::scaled(colors::phosphor, 0.8f);
            drawRect(lines, *btns[i], c, 1.5f);
            r.drawTextCentered(labels[i], btns[i]->pos + btns[i]->size * 0.5f, kButtonTextHeight, c, 1.3f);
        }
    }

    if (state.shipDestroyed) {
        r.drawTextCentered("SHIP DESTROYED", screen_ * 0.5f - glm::vec2(0.0f, 20.0f), 44.0f, colors::red, 2.4f);
        r.drawTextCentered("CLICK TO RESTART", screen_ * 0.5f + glm::vec2(0.0f, 36.0f), 16.0f, colors::amber, 1.5f);
    }
}

} // namespace ee
