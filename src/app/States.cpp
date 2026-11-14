#include "app/States.h"

#include "core/Random.h"
#include "vector/Shapes.h"

#include <algorithm>
#include <cmath>
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
    : r_(r), ship_(shapes::ship()), planet_(shapes::planet()) {
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

    camera_.snapTo(shipPos_, 1.0f);
}

HudState PlayingState::hudState() const {
    HudState s;
    s.zoom = camera_.current().zoom;
    s.zoomUnit = Camera2D::zoomToUnit(camera_.targetZoom());
    s.bloom = r_.bloomQuality();
    s.rocks = rockCount_;
    s.mouseWorld = camera_.current().screenToWorld(mouse_);
    return s;
}

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
    if (ev.recenter) camera_.moveTo(shipPos_);
    if (ev.zoomUnit >= 0.0f) camera_.setZoomAt(screen * 0.5f, Camera2D::unitToZoom(ev.zoomUnit));

    // Drag-to-pan: middle button at once, left button after a small threshold
    // (so a plain left click stays available for selecting things in M4).
    if (input.pressed[InputState::Left] && !ev.consumedMouse) {
        leftOnWorld_ = true;
        leftTravel_ = 0.0f;
    }
    if (!input.down[InputState::Left]) leftOnWorld_ = false;
    if (input.pressed[InputState::Middle] && !ev.consumedMouse) middleOnWorld_ = true;
    if (!input.down[InputState::Middle]) middleOnWorld_ = false;

    const glm::vec2 delta(input.deltaX, input.deltaY);
    if (leftOnWorld_) {
        leftTravel_ += glm::length(delta);
        if (leftTravel_ > kDragThreshold) camera_.panByScreenDelta(delta);
    }
    if (middleOnWorld_) camera_.panByScreenDelta(delta);

    if (input.wheelY != 0.0f)
        camera_.zoomAt(mouse_, std::pow(kZoomPerWheelTick, input.wheelY));
}

void PlayingState::update(double dt, StateStack&) {
    prevSimTime_ = simTime_;
    simTime_ += dt;
    camera_.update(dt);
}

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
        drawShapeLod(lines, v, planet_, Transform2D{.position = p.pos, .scale = p.radius}, p.radius,
                     p.color, 2.0f);

    for (int i = 0; i < rockCount_; ++i) {
        const Rock& rock = rocks_[static_cast<std::size_t>(i)];
        drawShapeLod(lines, v, rockShapes_[static_cast<std::size_t>(rock.variant)],
                     Transform2D{.position = rock.pos,
                                 .rotation = static_cast<float>(t) * rock.spin,
                                 .scale = rock.radius},
                     rock.radius, colors::amber, 1.5f);
    }

    // The ship turns to face the mouse. When zoomed far out it is drawn as an
    // enlarged icon so it never disappears.
    const glm::vec2 toMouse = v.screenToWorld(mouse_) - shipPos_;
    const float shipScale = std::max(28.0f, 9.0f / v.zoom);
    lines.shape(ship_,
                Transform2D{.position = shipPos_,
                            .rotation = std::atan2(toMouse.y, toMouse.x),
                            .scale = shipScale},
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
