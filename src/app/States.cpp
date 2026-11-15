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
constexpr int kStarCount = 500;
constexpr int kMaxLoadRadius = 4;      // never keep more than 9x9 sectors around the ship
constexpr int kStreamBudget = 4;       // non-essential sectors generated per simulation step
constexpr float kOrbitMaxZoom = 0.6f;  // orbit rings are hidden when zoomed closer (polygon edges show)
constexpr float kLabelMinSectorPixels = 150.0f;
constexpr float kGridBaseSpacing = 500.0f;
constexpr float kMinGridPixels = 24.0f;
constexpr float kDotRadiusPixels = 3.0f; // below this on-screen radius, draw a dot instead
constexpr float kMinDrawPixels = 0.6f;   // rocks/debris smaller than this are not drawn at all
constexpr float kDragThreshold = 4.0f;
constexpr float kZoomPerWheelTick = 1.15f;
constexpr float kShipScale = 28.0f;
constexpr float kShipIconPixels = 9.0f; // minimum on-screen size of the ship
constexpr float kPickPixels = 10.0f;    // click tolerance around objects
constexpr double kDoubleClickSeconds = 0.35;
constexpr float kDoubleClickDistance = 8.0f;
constexpr float kStandoffDistance = 50.0f; // how far from a surface "fly to" stops
constexpr double kStep = 1.0 / 60.0;

glm::vec4 tintColor(Tint t) {
    switch (t) {
    case Tint::Cyan: return colors::cyan;
    case Tint::Amber: return colors::amber;
    case Tint::Phosphor: return colors::phosphor;
    case Tint::Red: return colors::red;
    case Tint::White: return colors::white;
    }
    return colors::white;
}

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
PlayingState::PlayingState(Renderer& r, std::uint64_t galaxySeed)
    : r_(r), galaxy_(makeGalaxy(galaxySeed)), streamer_(galaxySeed), shipShape_(shapes::ship()),
      planetShape_(shapes::planet()), circleShape_(shapes::circle(24)),
      orbitShape_(shapes::circle(96)), diamondShape_(shapes::ngon(4)),
      starCrossShape_(shapes::starCross()) {
    for (int i = 0; i < kShapeVariants; ++i)
        rockShapes_.push_back(shapes::asteroid(static_cast<std::uint64_t>(i) + 1));

    Random rng(12345);
    for (int i = 0; i < kStarCount; ++i)
        stars_.push_back({{rng.nextFloat(), rng.nextFloat()}, rng.range(0.01f, 0.08f),
                          rng.range(0.25f, 0.9f)});

    ship_.teleport({0.0f, 0.0f}, -kPi / 2.0f);
    camera_.snapTo(ship_.position(), 1.0f);
    streamer_.update(origin_, origin_, origin_, 0);
}

glm::dvec2 PlayingState::absolutePosition(glm::vec2 local) const {
    return glm::dvec2(origin_.x, origin_.y) * static_cast<double>(kSectorSize) + glm::dvec2(local);
}

void PlayingState::visibleRange(const CameraView& v, SectorCoord& lo, SectorCoord& hi) const {
    const glm::vec2 tl = v.screenToWorld({0.0f, 0.0f});
    const glm::vec2 br = v.screenToWorld(v.viewport);
    const auto index = [](float w) { return static_cast<int>(std::floor((w + kSectorHalf) / kSectorSize)); };
    lo = {std::clamp(origin_.x + index(tl.x), origin_.x - kMaxLoadRadius, origin_.x + kMaxLoadRadius),
          std::clamp(origin_.y + index(tl.y), origin_.y - kMaxLoadRadius, origin_.y + kMaxLoadRadius)};
    hi = {std::clamp(origin_.x + index(br.x), origin_.x - kMaxLoadRadius, origin_.x + kMaxLoadRadius),
          std::clamp(origin_.y + index(br.y), origin_.y - kMaxLoadRadius, origin_.y + kMaxLoadRadius)};
}

// Keep the ship's coordinates small: when it leaves the origin sector, move the origin to
// the sector it entered and shift everything else by the opposite amount.
void PlayingState::rebaseOrigin() {
    const glm::vec2 p = ship_.position();
    const int dx = static_cast<int>(std::floor((p.x + kSectorHalf) / kSectorSize));
    const int dy = static_cast<int>(std::floor((p.y + kSectorHalf) / kSectorSize));
    if (dx == 0 && dy == 0) return;
    const glm::vec2 shift(-static_cast<float>(dx) * kSectorSize, -static_cast<float>(dy) * kSectorSize);
    origin_.x += dx;
    origin_.y += dy;
    ship_.shiftOrigin(shift);
    camera_.shiftOrigin(shift);
    exhaust_.shiftOrigin(shift);
}

// ---- Selection ----------------------------------------------------------
PlayingState::Selection PlayingState::pick(const CameraView& v, glm::vec2 screenPoint) const {
    const glm::vec2 world = v.screenToWorld(screenPoint);
    const float tolerance = kPickPixels / v.zoom;

    SectorCoord lo, hi;
    visibleRange(v, lo, hi);

    Selection best;
    float bestScore = 1e30f;
    for (int sy = lo.y; sy <= hi.y; ++sy)
        for (int sx = lo.x; sx <= hi.x; ++sx) {
            const Sector* sector = streamer_.find({sx, sy});
            if (!sector) continue;
            const glm::vec2 offset = sectorOffset(sector->coord, origin_);
            for (std::size_t i = 0; i < sector->objects.size(); ++i) {
                const SectorObject& o = sector->objects[i];
                const float d = glm::length(world - (o.pos + offset));
                if (d > std::max(o.radius, tolerance)) continue;
                // Prefer the object whose edge is nearest; nebulae are a backdrop, so
                // anything inside one wins over it.
                float score = d - o.radius;
                if (o.kind == ObjectKind::Nebula) score += 1.0e5f;
                if (score < bestScore) {
                    bestScore = score;
                    best = {true, sector->coord, i};
                }
            }
        }
    return best;
}

const SectorObject* PlayingState::selectedObject() const {
    if (!selection_.valid) return nullptr;
    const Sector* s = streamer_.find(selection_.sector);
    if (!s || selection_.index >= s->objects.size()) return nullptr;
    return &s->objects[selection_.index];
}

glm::vec2 PlayingState::selectionPos() const {
    return selectedObject()->pos + sectorOffset(selection_.sector, origin_);
}

std::string PlayingState::selectionName() const {
    return std::string(objectKindName(selectedObject()->kind)) + " " +
           std::to_string(selection_.sector.x) + "," + std::to_string(selection_.sector.y) + "-" +
           std::to_string(selection_.index + 1);
}

void PlayingState::flyToSelection() {
    const SectorObject* o = selectedObject();
    if (!o) return;
    glm::vec2 away = ship_.position() - selectionPos();
    const float len = glm::length(away);
    away = len > 1e-3f ? away / len : glm::vec2(1.0f, 0.0f);
    const float standoff = o->kind == ObjectKind::Nebula ? 0.0f : o->radius + kStandoffDistance;
    ship_.setDestination(selectionPos() + away * standoff);
}

HudState PlayingState::hudState() const {
    HudState s;
    const CameraView cv = camera_.current();
    s.zoom = cv.zoom;
    s.zoomUnit = Camera2D::zoomToUnit(camera_.targetZoom());
    s.bloom = r_.bloomQuality();
    s.objects = static_cast<int>(streamer_.loadedObjectCount());
    s.sectorsLoaded = static_cast<int>(streamer_.loadedCount());
    s.follow = follow_;
    s.showGrid = showGrid_;
    s.dragAssist = ship_.dragAssist();
    const glm::dvec2 abs = absolutePosition(cv.screenToWorld(mouse_));
    s.mouseWorld = {static_cast<float>(abs.x), static_cast<float>(abs.y)};

    char buf[160];
    std::snprintf(buf, sizeof buf, "SECTOR %d,%d\n", origin_.x, origin_.y);
    s.info = buf;
    s.info += homeHintText(homeHint(galaxy_, origin_));
    std::snprintf(buf, sizeof buf, "\n\nSPEED %.0f", static_cast<double>(ship_.speed()));
    s.info += buf;
    if (ship_.hasDestination()) {
        std::snprintf(buf, sizeof buf, "\nDESTINATION %.0f", static_cast<double>(ship_.distanceToDestination()));
        s.info += buf;
    }
    if (const SectorObject* o = selectedObject()) {
        const float dist = glm::length(selectionPos() - ship_.position());
        std::snprintf(buf, sizeof buf, "\n\nSELECTED %s\nDISTANCE %.0f  RADIUS %.0f",
                      selectionName().c_str(), static_cast<double>(dist),
                      static_cast<double>(o->radius));
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
    if (ev.toggleGrid) showGrid_ = !showGrid_;
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
            selection_ = pick(camera_.current(), mouse_);
            const bool doubleClick = simTime_ - lastClickTime_ < kDoubleClickSeconds &&
                                     glm::length(mouse_ - lastClickPos_) < kDoubleClickDistance;
            if (doubleClick && selectedObject()) flyToSelection();
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
    rebaseOrigin();

    if (follow_) camera_.setFollowTarget(ship_.position());
    camera_.update(dt);

    SectorCoord lo, hi;
    visibleRange(camera_.current(), lo, hi);
    streamer_.update(origin_, lo, hi, kStreamBudget);
}

// ---- Rendering ----------------------------------------------------------
void PlayingState::drawStars(const CameraView& v) {
    LineBatch& lines = r_.lines();
    lines.setView(r_.screenView());
    const glm::dvec2 cam = absolutePosition(v.center);
    const glm::dvec2 vp(v.viewport);
    for (const Star& s : stars_) {
        // Stars drift slower than the world to suggest depth, and wrap around the screen.
        // Computed in double: the absolute camera position can be hundreds of thousands of units.
        const glm::dvec2 raw = glm::dvec2(s.unitPos) * vp - cam * static_cast<double>(v.zoom * s.depth);
        const glm::vec2 p(static_cast<float>(std::fmod(std::fmod(raw.x, vp.x) + vp.x, vp.x)),
                          static_cast<float>(std::fmod(std::fmod(raw.y, vp.y) + vp.y, vp.y)));
        lines.line(p, p, colors::scaled(colors::white, s.brightness * 0.6f), 1.0f + s.depth * 15.0f);
    }
}

void PlayingState::drawGrid(const CameraView& v) {
    float spacing = kGridBaseSpacing;
    while (spacing * v.zoom < kMinGridPixels) spacing *= 10.0f;

    const glm::vec2 tl = v.screenToWorld({0.0f, 0.0f});
    const glm::vec2 br = v.screenToWorld(v.viewport);
    const glm::vec4 color = colors::scaled(colors::cyan, 0.14f);

    // Grid lines sit at multiples of `spacing` in absolute coordinates, so they do not
    // jump when the origin moves to a new sector.
    const glm::dvec2 off = absolutePosition({0.0f, 0.0f});
    const double sp = spacing;
    LineBatch& lines = r_.lines();
    for (double k = std::floor((tl.x + off.x) / sp); k * sp - off.x <= br.x; k += 1.0) {
        const float x = static_cast<float>(k * sp - off.x);
        lines.line({x, tl.y}, {x, br.y}, color, 1.0f);
    }
    for (double k = std::floor((tl.y + off.y) / sp); k * sp - off.y <= br.y; k += 1.0) {
        const float y = static_cast<float>(k * sp - off.y);
        lines.line({tl.x, y}, {br.x, y}, color, 1.0f);
    }
}

void PlayingState::drawSectorBorders(const CameraView& v) {
    const glm::vec2 tl = v.screenToWorld({0.0f, 0.0f});
    const glm::vec2 br = v.screenToWorld(v.viewport);
    const glm::vec4 color = colors::scaled(colors::amber, 0.45f);
    LineBatch& lines = r_.lines();
    for (float i = std::ceil((tl.x + kSectorHalf) / kSectorSize);; i += 1.0f) {
        const float x = i * kSectorSize - kSectorHalf;
        if (x > br.x) break;
        lines.line({x, tl.y}, {x, br.y}, color, 1.5f);
    }
    for (float i = std::ceil((tl.y + kSectorHalf) / kSectorSize);; i += 1.0f) {
        const float y = i * kSectorSize - kSectorHalf;
        if (y > br.y) break;
        lines.line({tl.x, y}, {br.x, y}, color, 1.5f);
    }
}

void PlayingState::drawSectorLabels(const CameraView& v) {
    if (kSectorSize * v.zoom < kLabelMinSectorPixels) return;
    SectorCoord lo, hi;
    visibleRange(v, lo, hi);
    r_.lines().setView(r_.screenView());
    char buf[32];
    for (int sy = lo.y; sy <= hi.y; ++sy)
        for (int sx = lo.x; sx <= hi.x; ++sx) {
            const glm::vec2 corner =
                v.worldToScreen(sectorOffset({sx, sy}, origin_) + glm::vec2(-kSectorHalf, -kSectorHalf));
            std::snprintf(buf, sizeof buf, "%d,%d", sx, sy);
            const bool home = SectorCoord{sx, sy} == galaxy_.home;
            r_.drawText(home ? std::string("HOME ") + buf : std::string(buf), corner + glm::vec2(8.0f, 8.0f),
                        10.0f, colors::scaled(home ? colors::phosphor : colors::amber, 0.7f), 1.0f);
        }
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

void PlayingState::drawObject(const CameraView& v, const SectorObject& o, glm::vec2 offset, double t) {
    LineBatch& lines = r_.lines();
    const glm::vec2 pos = o.pos + offset;
    const glm::vec4 color = tintColor(o.tint);
    const float ft = static_cast<float>(t);
    const float screenR = v.screenRadius(o.radius);

    switch (o.kind) {
    case ObjectKind::Star:
        if (!v.isVisible(pos, o.radius * 1.8f)) return;
        if (screenR < kDotRadiusPixels) {
            lines.line(pos, pos, color, 5.0f);
            return;
        }
        lines.shape(circleShape_, Transform2D{.position = pos, .scale = o.radius}, color, 2.5f);
        lines.shape(circleShape_, Transform2D{.position = pos, .scale = o.radius * 1.3f},
                    colors::scaled(color, 0.35f), 1.5f);
        lines.shape(starCrossShape_,
                    Transform2D{.position = pos, .rotation = ft * 0.05f, .scale = o.radius * 1.8f},
                    colors::scaled(color, 0.8f), 1.5f);
        break;
    case ObjectKind::Planet:
        drawShapeLod(lines, v, planetShape_, Transform2D{.position = pos, .scale = o.radius},
                     o.radius, color, 2.0f);
        break;
    case ObjectKind::Rock:
    case ObjectKind::Debris:
        if (screenR < kMinDrawPixels) return;
        drawShapeLod(lines, v, rockShapes_[static_cast<std::size_t>(o.variant)],
                     Transform2D{.position = pos, .rotation = ft * o.spin, .scale = o.radius},
                     o.radius, o.kind == ObjectKind::Debris ? colors::scaled(color, 0.55f) : color, 1.5f);
        break;
    case ObjectKind::Derelict:
        drawShapeLod(lines, v, shipShape_,
                     Transform2D{.position = pos,
                                 .rotation = static_cast<float>(o.variant) + ft * o.spin,
                                 .scale = o.radius * 1.4f},
                     o.radius * 1.4f, colors::scaled(color, 0.7f), 1.6f);
        break;
    case ObjectKind::Anomaly: {
        if (!v.isVisible(pos, o.radius * 2.0f)) return;
        if (screenR < kDotRadiusPixels) {
            lines.line(pos, pos, colors::cyan, 4.0f);
            return;
        }
        for (int k = 0; k < 3; ++k) {
            const float phase = std::fmod(ft * 0.5f + static_cast<float>(k) / 3.0f, 1.0f);
            lines.shape(diamondShape_,
                        Transform2D{.position = pos, .rotation = phase * 1.5f,
                                    .scale = o.radius * (0.3f + 1.7f * phase)},
                        colors::scaled(color, 1.0f - phase), 1.5f);
        }
        lines.shape(circleShape_, Transform2D{.position = pos, .scale = o.radius * 0.25f}, colors::white, 1.5f);
        break;
    }
    case ObjectKind::Nebula:
        if (!v.isVisible(pos, o.radius) || screenR < 3.0f) return;
        for (int k = 0; k < 4; ++k)
            lines.shape(rockShapes_[static_cast<std::size_t>((o.variant + k) % kShapeVariants)],
                        Transform2D{.position = pos, .rotation = static_cast<float>(k) * 1.3f,
                                    .scale = o.radius * (1.0f - 0.22f * static_cast<float>(k))},
                        colors::scaled(color, 0.28f - 0.05f * static_cast<float>(k)), 1.5f);
        break;
    }
}

void PlayingState::drawSector(const CameraView& v, const Sector& sector, double t) {
    const glm::vec2 offset = sectorOffset(sector.coord, origin_);
    if (!v.isVisible(offset, kSectorHalf * 1.42f)) return;

    if (sector.starIndex >= 0 && v.zoom < kOrbitMaxZoom) {
        const glm::vec2 starPos = sector.objects[static_cast<std::size_t>(sector.starIndex)].pos;
        for (const SectorObject& o : sector.objects) {
            if (o.kind != ObjectKind::Planet) continue;
            const float orbit = glm::length(o.pos - starPos);
            if (v.screenRadius(orbit) < 12.0f) continue;
            r_.lines().shape(orbitShape_, Transform2D{.position = starPos + offset, .scale = orbit},
                             colors::scaled(colors::cyan, 0.10f), 1.0f);
        }
    }
    for (const SectorObject& o : sector.objects) drawObject(v, o, offset, t);
}

void PlayingState::render(double alpha) {
    const double t = prevSimTime_ + (simTime_ - prevSimTime_) * alpha;
    const CameraView v = camera_.view(alpha);
    LineBatch& lines = r_.lines();

    drawStars(v);

    lines.setView(r_.worldView(v.center, v.zoom));
    if (showGrid_) {
        drawGrid(v);
        drawSectorBorders(v);
    }

    SectorCoord lo, hi;
    visibleRange(v, lo, hi);
    for (int sy = lo.y; sy <= hi.y; ++sy)
        for (int sx = lo.x; sx <= hi.x; ++sx)
            if (const Sector* s = streamer_.find({sx, sy})) drawSector(v, *s, t);

    // Selection ring (at least 12 screen points wide so small objects stay findable).
    if (const SectorObject* sel = selectedObject()) {
        const float ringRadius = std::max(sel->radius * 1.35f, 12.0f / v.zoom);
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

    if (showGrid_) drawSectorLabels(v);
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
