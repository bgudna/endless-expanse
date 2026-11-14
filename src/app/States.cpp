#include "app/States.h"

#include "core/MathUtil.h"
#include "core/Random.h"
#include "vector/Shapes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace ee {

namespace {
constexpr int kMaxRocks = 6000;
constexpr int kRockStep = 100;
constexpr int kStarCount = 500;
constexpr int kRockVariants = 8;
constexpr float kSectorHalfWidth = 3000.0f;
constexpr float kSectorHalfHeight = 1800.0f;
constexpr float kGridBaseSpacing = 500.0f;
constexpr float kMinGridPixels = 24.0f;
constexpr float kDotRadiusPixels = 3.0f; // below this on-screen radius, draw a dot instead
constexpr float kDragThreshold = 4.0f;
constexpr float kZoomPerWheelTick = 1.15f;
constexpr float kShipScale = 28.0f;
constexpr float kShipIconPixels = 9.0f; // minimum on-screen size of the ship
constexpr float kPickPixels = 10.0f;    // click tolerance around objects
constexpr double kDoubleClickSeconds = 0.35;
constexpr float kDoubleClickDistance = 8.0f;
constexpr float kStandoffDistance = 50.0f; // how far from a surface "fly to" stops
constexpr double kStep = 1.0 / 60.0;

// Draw a shape with culling and level-of-detail: off-screen shapes are skipped and
// tiny ones collapse to a single dot.
void drawShapeLod(LineBatch& lines, const CameraView& v, const VectorShape& shape,
                  const Transform2D& t, float worldRadius, const glm::vec4& color, float width) {
    if (!v.isVisible(t.position, worldRadius)) return;
    if (v.screenRadius(worldRadius) < kDotRadiusPixels)
        lines.line(t.position, t.position, color, 2.5f);
    else
        lines.shape(shape, t, color, width);
}
} // namespace

// ---- Boot ---------------------------------------------------------------
void BootState::update(double dt, StateStack& stack) {
    elapsed_ += dt;
    if (elapsed_ >= 0.5) stack.replace(std::make_unique<MainMenuState>(r_));
}
void BootState::render(double) {}

// ---- Main menu ----------------------------------------------------------
MainMenuState::MainMenuState(Renderer& r) : r_(r), ship_(shapes::ship()) {}

void MainMenuState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Left]) stack.replace(std::make_unique<PlayingState>(r_));
}

void MainMenuState::update(double dt, StateStack&) {
    prevTime_ = time_;
    time_ += dt;
}

void MainMenuState::render(double alpha) {
    const double t = prevTime_ + (time_ - prevTime_) * alpha;
    const glm::vec2 size = r_.logicalSize();
    const glm::vec2 center = size * 0.5f;

    r_.lines().setView(r_.screenView());
    r_.lines().shape(ship_, Transform2D{.position = center + glm::vec2(0.0f, 20.0f),
                                        .rotation = static_cast<float>(t) * 0.6f,
                                        .scale = 60.0f},
                     colors::phosphor, 2.0f);

    r_.drawTextCentered("ENDLESS EXPANSE", center - glm::vec2(0.0f, 140.0f), 54.0f, colors::cyan,
                        2.5f);
    if (std::fmod(t, 1.2) < 0.8)
        r_.drawTextCentered("CLICK TO START", center + glm::vec2(0.0f, 140.0f), 20.0f,
                            colors::amber, 1.8f);
}

// ---- Playing ------------------------------------------------------------
PlayingState::PlayingState(Renderer& r)
    : r_(r), shipShape_(shapes::ship()), planetShape_(shapes::planet()),
      circleShape_(shapes::circle(24)), diamondShape_(shapes::ngon(4)) {
    for (int i = 0; i < kRockVariants; ++i)
        rockShapes_.push_back(shapes::asteroid(static_cast<std::uint64_t>(i) + 1));

    Random rng(12345);
    for (int i = 0; i < kStarCount; ++i)
        stars_.push_back({{rng.nextFloat(), rng.nextFloat()}, rng.range(0.01f, 0.08f),
                          rng.range(0.25f, 0.9f)});
    for (int i = 0; i < kMaxRocks; ++i)
        rocks_.push_back({{rng.range(-kSectorHalfWidth, kSectorHalfWidth),
                           rng.range(-kSectorHalfHeight, kSectorHalfHeight)},
                          rng.range(8.0f, 28.0f), rng.range(-1.0f, 1.0f),
                          static_cast<int>(rng.nextU64() % kRockVariants)});

    planets_ = {{{-250.0f, -120.0f}, 90.0f, colors::cyan},
                {{1400.0f, 600.0f}, 220.0f, colors::amber},
                {{-1800.0f, 900.0f}, 140.0f, colors::phosphor},
                {{900.0f, -1300.0f}, 70.0f, colors::red},
                {{-900.0f, -1200.0f}, 180.0f, colors::cyan}};

    ship_.teleport({0.0f, 0.0f}, -kPi / 2.0f);
    camera_.snapTo(ship_.position(), 1.0f);
}

// ---- Selection ----------------------------------------------------------
PlayingState::Selection PlayingState::pick(const CameraView& v, glm::vec2 screenPoint) const {
    const glm::vec2 world = v.screenToWorld(screenPoint);
    const float tolerance = kPickPixels / v.zoom;

    Selection best;
    float bestScore = 1e30f;
    auto consider = [&](Selection::Kind kind, std::size_t index, glm::vec2 pos, float radius) {
        const float d = glm::length(world - pos);
        if (d > std::max(radius, tolerance)) return;
        const float score = d - radius; // prefer the object whose edge is nearest
        if (score < bestScore) {
            bestScore = score;
            best = {kind, index};
        }
    };
    for (std::size_t i = 0; i < planets_.size(); ++i)
        consider(Selection::Kind::Planet, i, planets_[i].pos, planets_[i].radius);
    for (std::size_t i = 0; i < static_cast<std::size_t>(rockCount_); ++i)
        consider(Selection::Kind::Rock, i, rocks_[i].pos, rocks_[i].radius);
    return best;
}

bool PlayingState::selectionValid() const {
    switch (selection_.kind) {
    case Selection::Kind::Planet: return selection_.index < planets_.size();
    case Selection::Kind::Rock: return selection_.index < static_cast<std::size_t>(rockCount_);
    case Selection::Kind::None: return false;
    }
    return false;
}

glm::vec2 PlayingState::selectionPos() const {
    return selection_.kind == Selection::Kind::Planet ? planets_[selection_.index].pos
                                                      : rocks_[selection_.index].pos;
}

float PlayingState::selectionRadius() const {
    return selection_.kind == Selection::Kind::Planet ? planets_[selection_.index].radius
                                                      : rocks_[selection_.index].radius;
}

std::string PlayingState::selectionName() const {
    return std::string(selection_.kind == Selection::Kind::Planet ? "PLANET " : "ROCK ") +
           std::to_string(selection_.index + 1);
}

void PlayingState::flyToSelection() {
    if (!selectionValid()) return;
    glm::vec2 away = ship_.position() - selectionPos();
    const float len = glm::length(away);
    away = len > 1e-3f ? away / len : glm::vec2(1.0f, 0.0f);
    ship_.setDestination(selectionPos() + away * (selectionRadius() + kStandoffDistance));
}

HudState PlayingState::hudState() const {
    HudState s;
    const CameraView cv = camera_.current();
    s.zoom = cv.zoom;
    s.zoomUnit = Camera2D::zoomToUnit(camera_.targetZoom());
    s.bloom = r_.bloomQuality();
    s.rocks = rockCount_;
    s.follow = follow_;
    s.dragAssist = ship_.dragAssist();
    s.mouseWorld = cv.screenToWorld(mouse_);

    char buf[160];
    std::snprintf(buf, sizeof buf, "SPEED %.0f", static_cast<double>(ship_.speed()));
    s.info = buf;
    if (ship_.hasDestination()) {
        std::snprintf(buf, sizeof buf, "\nDESTINATION %.0f", static_cast<double>(ship_.distanceToDestination()));
        s.info += buf;
    }
    if (selectionValid()) {
        const float dist = glm::length(selectionPos() - ship_.position());
        std::snprintf(buf, sizeof buf, "\n\nSELECTED %s\nDISTANCE %.0f  RADIUS %.0f",
                      selectionName().c_str(), static_cast<double>(dist),
                      static_cast<double>(selectionRadius()));
        s.info += buf;
    }
    return s;
}

// ---- Input --------------------------------------------------------------
void PlayingState::handleInput(const InputState& input, StateStack& stack) {
    mouse_ = {input.mouseX, input.mouseY};
    const glm::vec2 screen = r_.logicalSize();
    camera_.setViewport(screen);

    hud_.layout(screen, hudState());
    const HudEvents ev = hud_.handleInput(input);

    if (ev.pause) stack.push(std::make_unique<PausedState>(r_));
    if (ev.cycleBloom) r_.setBloomQuality(nextQuality(r_.bloomQuality()));
    if (ev.rockDelta != 0)
        rockCount_ = std::clamp(rockCount_ + ev.rockDelta * kRockStep, 0, kMaxRocks);
    if (ev.toggleFollow) follow_ = !follow_;
    if (ev.toggleAssist) ship_.setDragAssist(!ship_.dragAssist());
    if (ev.zoomUnit >= 0.0f) camera_.setZoomAt(screen * 0.5f, Camera2D::unitToZoom(ev.zoomUnit));

    const glm::vec2 delta(input.deltaX, input.deltaY);

    // Left button: a short click selects (double-click flies there); a drag pans.
    if (input.pressed[InputState::Left] && !ev.consumedMouse) {
        leftOnWorld_ = true;
        leftTravel_ = 0.0f;
    }
    if (leftOnWorld_ && input.down[InputState::Left]) {
        leftTravel_ += glm::length(delta);
        if (leftTravel_ > kDragThreshold) {
            camera_.panByScreenDelta(delta);
            follow_ = false;
        }
    }
    if (leftOnWorld_ && input.released[InputState::Left]) {
        if (leftTravel_ <= kDragThreshold) {
            const Selection hit = pick(camera_.current(), mouse_);
            const bool doubleClick = simTime_ - lastClickTime_ < kDoubleClickSeconds &&
                                     glm::length(mouse_ - lastClickPos_) < kDoubleClickDistance;
            selection_ = hit;
            if (doubleClick && selectionValid()) flyToSelection();
            lastClickTime_ = doubleClick ? -10.0 : simTime_;
            lastClickPos_ = mouse_;
        }
        leftOnWorld_ = false;
    }
    if (!input.down[InputState::Left]) leftOnWorld_ = false;

    // Middle button always pans.
    if (input.pressed[InputState::Middle] && !ev.consumedMouse) middleOnWorld_ = true;
    if (!input.down[InputState::Middle]) middleOnWorld_ = false;
    if (middleOnWorld_) {
        camera_.panByScreenDelta(delta);
        follow_ = false;
    }

    // Right button: fly to the clicked point; hold it to keep steering toward the cursor.
    if (input.pressed[InputState::Right] && !ev.consumedMouse) rightOnWorld_ = true;
    if (!input.down[InputState::Right]) rightOnWorld_ = false;
    if (rightOnWorld_) ship_.setDestination(camera_.current().screenToWorld(mouse_));

    // Wheel zoom: around the ship while following, otherwise toward the cursor.
    if (input.wheelY != 0.0f)
        camera_.zoomAt(follow_ ? screen * 0.5f : mouse_, std::pow(kZoomPerWheelTick, input.wheelY));
}

void PlayingState::update(double dt, StateStack&) {
    prevSimTime_ = simTime_;
    simTime_ += dt;

    ship_.update(dt);
    if (ship_.thrusting()) {
        const glm::vec2 back = -ship_.forward();
        for (int i = 0; i < 2; ++i)
            exhaust_.emit(ship_.position() + back * (kShipScale * 0.5f), back, ship_.velocity(),
                          rng_);
    }
    exhaust_.update(static_cast<float>(dt));

    if (follow_) camera_.setFollowTarget(ship_.position());
    camera_.update(dt);
}

// ---- Rendering ----------------------------------------------------------
void PlayingState::drawStars(const CameraView& v) {
    LineBatch& lines = r_.lines();
    lines.setView(r_.screenView());
    for (const Star& s : stars_) {
        // Stars drift slower than the world to suggest depth, and wrap around the screen.
        const glm::vec2 offset = -v.center * v.zoom * s.depth;
        glm::vec2 p = s.unitPos * v.viewport + offset;
        p.x = std::fmod(std::fmod(p.x, v.viewport.x) + v.viewport.x, v.viewport.x);
        p.y = std::fmod(std::fmod(p.y, v.viewport.y) + v.viewport.y, v.viewport.y);
        lines.line(p, p, colors::scaled(colors::white, s.brightness * 0.6f), 1.0f + s.depth * 15.0f);
    }
}

void PlayingState::drawGrid(const CameraView& v) {
    float spacing = kGridBaseSpacing;
    while (spacing * v.zoom < kMinGridPixels) spacing *= 10.0f;

    const glm::vec2 tl = v.screenToWorld({0.0f, 0.0f});
    const glm::vec2 br = v.screenToWorld(v.viewport);
    const glm::vec4 color = colors::scaled(colors::cyan, 0.14f);

    LineBatch& lines = r_.lines();
    for (float x = std::floor(tl.x / spacing) * spacing; x <= br.x; x += spacing)
        lines.line({x, tl.y}, {x, br.y}, color, 1.0f);
    for (float y = std::floor(tl.y / spacing) * spacing; y <= br.y; y += spacing)
        lines.line({tl.x, y}, {br.x, y}, color, 1.0f);
}

void PlayingState::drawDestination(const CameraView& v, double t) {
    if (!ship_.hasDestination()) return;
    LineBatch& lines = r_.lines();
    const glm::vec2 from = ship_.position();
    const glm::vec2 to = ship_.destination();
    const glm::vec2 d = to - from;
    const float len = glm::length(d);

    // Dashed path preview with screen-constant dash length.
    if (len > 1.0f) {
        const glm::vec2 dir = d / len;
        float dash = 12.0f / v.zoom;
        if (len / (2.0f * dash) > 300.0f) dash = len / 600.0f; // cap the dash count
        const glm::vec4 color = colors::scaled(colors::cyan, 0.6f);
        for (float s = 0.0f; s < len; s += 2.0f * dash)
            lines.line(from + dir * s, from + dir * std::min(s + dash, len), color, 1.5f);
    }

    // Destination marker: a slowly turning diamond with a centre ring.
    const float unit = 1.0f / v.zoom; // one screen point in world units
    lines.shape(diamondShape_,
                Transform2D{.position = to, .rotation = static_cast<float>(t) * 1.5f,
                            .scale = 10.0f * unit},
                colors::amber, 1.8f);
    lines.shape(circleShape_, Transform2D{.position = to, .scale = 4.0f * unit}, colors::amber, 1.5f);
}

void PlayingState::render(double alpha) {
    const double t = prevSimTime_ + (simTime_ - prevSimTime_) * alpha;
    const CameraView v = camera_.view(alpha);
    LineBatch& lines = r_.lines();

    drawStars(v);

    lines.setView(r_.worldView(v.center, v.zoom));
    drawGrid(v);

    // Sector boundary.
    const glm::vec4 boundary = colors::scaled(colors::amber, 0.45f);
    const glm::vec2 a(-kSectorHalfWidth, -kSectorHalfHeight), b(kSectorHalfWidth, -kSectorHalfHeight);
    const glm::vec2 c(kSectorHalfWidth, kSectorHalfHeight), d(-kSectorHalfWidth, kSectorHalfHeight);
    lines.line(a, b, boundary, 1.5f);
    lines.line(b, c, boundary, 1.5f);
    lines.line(c, d, boundary, 1.5f);
    lines.line(d, a, boundary, 1.5f);

    for (const Planet& p : planets_)
        drawShapeLod(lines, v, planetShape_, Transform2D{.position = p.pos, .scale = p.radius},
                     p.radius, p.color, 2.0f);

    for (int i = 0; i < rockCount_; ++i) {
        const Rock& rock = rocks_[static_cast<std::size_t>(i)];
        drawShapeLod(lines, v, rockShapes_[static_cast<std::size_t>(rock.variant)],
                     Transform2D{.position = rock.pos,
                                 .rotation = static_cast<float>(t) * rock.spin,
                                 .scale = rock.radius},
                     rock.radius, colors::amber, 1.5f);
    }

    // Selection ring (at least 12 screen points wide so small objects stay findable).
    if (selectionValid()) {
        const float ringRadius = std::max(selectionRadius() * 1.35f, 12.0f / v.zoom);
        lines.shape(circleShape_,
                    Transform2D{.position = selectionPos(),
                                .rotation = static_cast<float>(t) * 0.5f,
                                .scale = ringRadius},
                    colors::white, 1.3f);
    }

    drawDestination(v, t);

    // Engine exhaust: short fading streaks, advanced by the render interpolation.
    if (v.zoom > 0.15f) {
        for (const ExhaustParticle& p : exhaust_.particles()) {
            const glm::vec2 pos = p.pos + p.vel * static_cast<float>(alpha * kStep);
            if (!v.isVisible(pos, 10.0f)) continue;
            const float fade = 1.0f - p.age / p.life;
            lines.line(pos, pos - p.vel * 0.04f, colors::scaled(colors::amber, fade), 1.5f);
        }
    }

    // Ship (interpolated). When zoomed far out it is drawn as an enlarged icon.
    const float af = static_cast<float>(alpha);
    const glm::vec2 shipPos =
        ship_.previousPosition() + (ship_.position() - ship_.previousPosition()) * af;
    const float shipHeading =
        ship_.previousHeading() + wrapAngle(ship_.heading() - ship_.previousHeading()) * af;
    lines.shape(shipShape_,
                Transform2D{.position = shipPos,
                            .rotation = shipHeading,
                            .scale = std::max(kShipScale, kShipIconPixels / v.zoom)},
                colors::phosphor, 2.5f);

    hud_.render(r_, hudState());
}

// ---- Paused -------------------------------------------------------------
void PausedState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Left]) stack.pop();
    if (input.pressed[InputState::Right]) {
        stack.clear();
        stack.push(std::make_unique<MainMenuState>(r_));
    }
}

void PausedState::render(double) {
    const glm::vec2 center = r_.logicalSize() * 0.5f;
    r_.drawTextCentered("PAUSED", center - glm::vec2(0.0f, 30.0f), 48.0f, colors::red, 2.5f);
    r_.drawTextCentered("LEFT CLICK: RESUME   RIGHT CLICK: MAIN MENU", center + glm::vec2(0.0f, 40.0f),
                        14.0f, colors::white, 1.4f);
}

} // namespace ee
