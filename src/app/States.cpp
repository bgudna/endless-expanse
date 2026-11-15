#include "app/States.h"

#include "app/Assets.h"
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
constexpr int kAlienShapes = 5;
constexpr float kMessageSeconds = 5.5f;
constexpr int kMaxLoadRadius = 4;      // never keep more than 9x9 sectors around the ship

constexpr float kOrbitMaxZoom = 0.6f;  // orbit rings are hidden when zoomed closer (polygon edges show)
constexpr float kLabelMinSectorPixels = 150.0f;
constexpr float kGridBaseSpacing = 500.0f;
constexpr float kMinGridPixels = 24.0f;
constexpr float kDotRadiusPixels = 3.0f; // below this on-screen radius, draw a dot instead
constexpr float kMinDrawPixels = 0.6f;   // rocks/debris smaller than this are not drawn at all
constexpr float kDragThreshold = 4.0f;
constexpr float kZoomPerWheelTick = 1.15f;
constexpr float kShipScale = 28.0f;
constexpr float kOrbitRadius = 120.0f; // how far the ship circles a right-clicked point
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
    : r_(r), seed_(galaxySeed), sim_(galaxySeed, loadAlienTypes()), shipShape_(shapes::ship()),
      planetShape_(shapes::planet()), circleShape_(shapes::circle(24)),
      orbitShape_(shapes::circle(96)), diamondShape_(shapes::ngon(4)),
      starCrossShape_(shapes::starCross()), mineShape_(shapes::mine()),
      holeShape_(shapes::blackHole()) {
    for (int i = 0; i < kShapeVariants; ++i)
        rockShapes_.push_back(shapes::asteroid(static_cast<std::uint64_t>(i) + 1));
    for (int i = 0; i < kAlienShapes; ++i) alienShapes_.push_back(shapes::alien(i));

    Random rng(12345);
    for (int i = 0; i < kStarCount; ++i)
        stars_.push_back({{rng.nextFloat(), rng.nextFloat()}, rng.range(0.01f, 0.08f),
                          rng.range(0.25f, 0.9f)});

    camera_.snapTo(sim_.player().position(), 1.0f);
}

void PlayingState::visibleRange(const CameraView& v, SectorCoord& lo, SectorCoord& hi) const {
    const SectorCoord origin = sim_.origin();
    const glm::vec2 tl = v.screenToWorld({0.0f, 0.0f});
    const glm::vec2 br = v.screenToWorld(v.viewport);
    const auto index = [](float w) { return static_cast<int>(std::floor((w + kSectorHalf) / kSectorSize)); };
    lo = {std::clamp(origin.x + index(tl.x), origin.x - kMaxLoadRadius, origin.x + kMaxLoadRadius),
          std::clamp(origin.y + index(tl.y), origin.y - kMaxLoadRadius, origin.y + kMaxLoadRadius)};
    hi = {std::clamp(origin.x + index(br.x), origin.x - kMaxLoadRadius, origin.x + kMaxLoadRadius),
          std::clamp(origin.y + index(br.y), origin.y - kMaxLoadRadius, origin.y + kMaxLoadRadius)};
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
            const Sector* sector = sim_.streamer().find({sx, sy});
            if (!sector) continue;
            const glm::vec2 offset = sectorOffset(sector->coord, sim_.origin());
            for (std::size_t i = 0; i < sector->objects.size(); ++i) {
                const SectorObject& o = sector->objects[i];
                if (sim_.isDestroyed(sector->coord, i)) continue;
                const float d = glm::length(world - (o.pos + offset));
                if (d > std::max(o.radius, tolerance)) continue;
                // Prefer the object whose edge is nearest; nebulae are a backdrop, so
                // anything inside one wins over it.
                float score = d - o.radius;
                if (o.kind == ObjectKind::Nebula) score += 1.0e5f;
                if (score < bestScore) {
                    bestScore = score;
                    best = {};
                    best.kind = Selection::Kind::Object;
                    best.sector = sector->coord;
                    best.index = i;
                }
            }
        }
    // Ships are the thing you most often want to click: they win ties against scenery.
    for (const AlienShip& a : sim_.aliens()) {
        const float d = glm::length(world - a.body.position());
        if (d > std::max(a.type->radius, tolerance * 1.5f)) continue;
        const float score = d - a.type->radius - 50.0f;
        if (score < bestScore) {
            bestScore = score;
            best = {};
            best.kind = Selection::Kind::Alien;
            best.alienId = a.id;
        }
    }
    return best;
}

const SectorObject* PlayingState::objectAt(const Selection& sel) const {
    if (sel.kind != Selection::Kind::Object) return nullptr;
    const Sector* s = sim_.streamer().find(sel.sector);
    if (!s || sel.index >= s->objects.size() || sim_.isDestroyed(sel.sector, sel.index)) return nullptr;
    return &s->objects[sel.index];
}

const AlienShip* PlayingState::alienAt(const Selection& sel) const {
    return sel.kind == Selection::Kind::Alien ? sim_.findAlien(sel.alienId) : nullptr;
}

std::optional<PlayingState::Located> PlayingState::locate(const Selection& sel) const {
    if (const AlienShip* a = alienAt(sel))
        return Located{a->body.position(), a->type->radius, false, a->body.velocity()};
    if (const SectorObject* o = objectAt(sel))
        return Located{o->pos + sectorOffset(sel.sector, sim_.origin()), o->radius,
                       o->kind == ObjectKind::Nebula, glm::vec2(0.0f)};
    return std::nullopt;
}

std::string PlayingState::selectionName(const Selection& sel) const {
    if (const AlienShip* a = alienAt(sel)) return a->type->name;
    if (const SectorObject* o = objectAt(sel))
        return std::string(objectKindName(o->kind)) + " " + std::to_string(sel.sector.x) + "," +
               std::to_string(sel.sector.y) + "-" + std::to_string(sel.index + 1);
    return "UNKNOWN";
}

// Orbit tracking: the orbit follows its target each step, so a moving ship is kept in orbit.
// Selecting something else does not change the orbit.
void PlayingState::startOrbit() {
    if (!hasSelection() || !sim_.playerAlive()) return;
    orbitSel_ = selection_;
    orbiting_ = true;
    updateOrbit();
}

void PlayingState::updateOrbit() {
    if (!orbiting_) return;
    if (!sim_.playerAlive()) {
        orbiting_ = false;
        return;
    }
    const std::optional<Located> t = locate(orbitSel_);
    if (!t) {
        orbiting_ = false;
        sim_.player().clearDestination();
        message_ = "ORBIT ENDED";
        messageAge_ = 0.0f;
        return;
    }
    // Circle at a standoff from the surface; nebulae are circled at their edge.
    const float radius = t->nebula ? t->radius : t->radius + kStandoffDistance;
    sim_.player().setOrbitDestination(t->pos, radius, t->velocity);
}

HudState PlayingState::hudState() const {
    HudState s;
    const Ship& ship = sim_.player();
    const CameraView cv = camera_.current();
    s.zoom = cv.zoom;
    s.zoomUnit = Camera2D::zoomToUnit(camera_.targetZoom());
    s.bloom = r_.bloomQuality();
    s.objects = static_cast<int>(sim_.streamer().loadedObjectCount());
    s.sectorsLoaded = static_cast<int>(sim_.streamer().loadedCount());
    s.follow = follow_;
    s.showGrid = showGrid_;
    s.dragAssist = ship.dragAssist();
    s.weaponsFree = sim_.weaponsFree();
    s.canOrbit = hasSelection();
    s.orbiting = orbiting_;
    s.hull = sim_.vitals().hullFraction();
    s.shield = sim_.vitals().shieldFraction();
    s.shipDestroyed = !sim_.playerAlive();
    s.message = message_;
    s.messageAlpha = std::clamp((kMessageSeconds - messageAge_) / 1.5f, 0.0f, 1.0f);
    if (sim_.playerInRadiation()) s.status = "RADIATION: SHIELDS DRAINING";
    else if (sim_.playerInNebula()) s.status = "NEBULA: SHIELDS OFFLINE, SENSORS DEGRADED";

    if (const AlienShip* h = sim_.hailingAlien()) {
        s.hailActive = true;
        s.hailTitle = h->type->name + " HAILING";
        if (h->type->role == Role::Distress) {
            s.hailText = "SURVIVORS ASK FOR ASSISTANCE. SHARE SUPPLIES?";
            s.hailAccept = "ASSIST";
        } else {
            s.hailText = "THEY OFFER TO TRADE SUPPLIES FOR REPAIRS.";
            s.hailAccept = "TRADE";
        }
    }

    const glm::dvec2 abs = sim_.absolutePosition(cv.screenToWorld(mouse_));
    s.mouseWorld = {static_cast<float>(abs.x), static_cast<float>(abs.y)};

    char buf[160];
    std::snprintf(buf, sizeof buf, "SECTOR %d,%d\n", sim_.origin().x, sim_.origin().y);
    s.info = buf;
    s.info += homeHintText(homeHint(sim_.galaxy(), sim_.origin()));
    std::snprintf(buf, sizeof buf, "\n\nSPEED %.0f", static_cast<double>(ship.speed()));
    s.info += buf;
    if (ship.hasDestination()) {
        std::snprintf(buf, sizeof buf, "\nDESTINATION %.0f", static_cast<double>(ship.distanceToDestination()));
        s.info += buf;
    }
    if (orbiting_) s.info += "\nORBITING " + selectionName(orbitSel_);
    if (const auto target = locate(selection_)) {
        const float dist = glm::length(target->pos - ship.position());
        std::snprintf(buf, sizeof buf, "\n\nSELECTED %s\nDISTANCE %.0f  RADIUS %.0f",
                      selectionName(selection_).c_str(), static_cast<double>(dist),
                      static_cast<double>(target->radius));
        s.info += buf;
        if (const AlienShip* a = alienAt(selection_)) {
            std::snprintf(buf, sizeof buf, "\n%s  %s\nHULL %.0f%%  SHIELD %.0f%%",
                          roleName(a->type->role), aiStateName(a->state),
                          static_cast<double>(a->vitals.hullFraction() * 100.0f),
                          static_cast<double>(a->vitals.shieldFraction() * 100.0f));
            s.info += buf;
            if (a->type->damage > 0.0f && dist > sim_.weapon().range) s.info += "\nOUT OF WEAPON RANGE";
        }
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

    if (!sim_.playerAlive() && input.pressed[InputState::Left] && !ev.consumedMouse) {
        stack.replace(std::make_unique<PlayingState>(r_, seed_));
        return;
    }

    if (ev.pause) stack.push(std::make_unique<PausedState>(r_));
    if (ev.cycleBloom) r_.setBloomQuality(nextQuality(r_.bloomQuality()));
    if (ev.toggleGrid) showGrid_ = !showGrid_;
    if (ev.toggleWeapons) sim_.setWeaponsFree(!sim_.weaponsFree());
    if (ev.hailAccept) sim_.answerHail(true);
    if (ev.hailDecline) sim_.answerHail(false);
    if (ev.toggleFollow) follow_ = !follow_;
    if (ev.toggleAssist) sim_.player().setDragAssist(!sim_.player().dragAssist());
    if (ev.orbitSelected) startOrbit();
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
            // The selected ship becomes the weapons target.
            sim_.setTarget(selection_.kind == Selection::Kind::Alien ? selection_.alienId : -1);
            const bool doubleClick = simTime_ - lastClickTime_ < kDoubleClickSeconds &&
                                     glm::length(mouse_ - lastClickPos_) < kDoubleClickDistance;
            if (doubleClick && hasSelection()) startOrbit();
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

    // Right button: fly to the clicked point and circle it; hold it to keep steering toward the cursor.
    if (input.pressed[InputState::Right] && !ev.consumedMouse) rightOnWorld_ = true;
    if (!input.down[InputState::Right]) rightOnWorld_ = false;
    if (rightOnWorld_ && sim_.playerAlive()) {
        orbiting_ = false;
        sim_.player().setOrbitDestination(camera_.current().screenToWorld(mouse_), kOrbitRadius);
    }

    // Wheel zoom: around the ship while following, otherwise toward the cursor.
    if (input.wheelY != 0.0f)
        camera_.zoomAt(follow_ ? screen * 0.5f : mouse_, std::pow(kZoomPerWheelTick, input.wheelY));
}

void PlayingState::processEvents() {
    for (const SimEvent& e : sim_.takeEvents()) {
        switch (e.type) {
        case SimEventType::Explosion:
            bursts_.push_back({e.pos, 0.0f, 0.8f, std::max(e.value, 30.0f)});
            break;
        case SimEventType::Hit:
            bursts_.push_back({e.pos, 0.0f, 0.25f, 14.0f});
            break;
        case SimEventType::Message:
            message_ = e.text;
            messageAge_ = 0.0f;
            break;
        default: break;
        }
    }
    if (bursts_.size() > 200) bursts_.erase(bursts_.begin(), bursts_.begin() + 100);
}

void PlayingState::update(double dt, StateStack&) {
    prevSimTime_ = simTime_;
    simTime_ += dt;
    const float fdt = static_cast<float>(dt);

    SectorCoord lo, hi;
    visibleRange(camera_.current(), lo, hi);
    sim_.setStreamBox(lo, hi);
    updateOrbit();
    sim_.update(dt);

    const glm::vec2 shift = sim_.takeOriginShift();
    if (shift != glm::vec2(0.0f)) {
        camera_.shiftOrigin(shift);
        exhaust_.shiftOrigin(shift);
        for (Burst& b : bursts_) b.pos += shift;
    }

    const Ship& ship = sim_.player();
    if (ship.thrusting() && sim_.playerAlive() && !ship.orbiting()) {
        const glm::vec2 back = -ship.forward();
        exhaust_.emit(ship.position() + back * (kShipScale * 0.5f));
    }
    exhaust_.update(fdt);

    processEvents();
    messageAge_ += fdt;
    for (Burst& b : bursts_) b.age += fdt;
    bursts_.erase(std::remove_if(bursts_.begin(), bursts_.end(), [](const Burst& b) { return b.age >= b.life; }),
                  bursts_.end());

    if (follow_) camera_.setFollowTarget(ship.position());
    camera_.update(dt);
}

// ---- Rendering ----------------------------------------------------------
void PlayingState::drawStars(const CameraView& v) {
    LineBatch& lines = r_.lines();
    lines.setView(r_.screenView());
    const glm::dvec2 cam = sim_.absolutePosition(v.center);
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
    const glm::dvec2 off = sim_.absolutePosition({0.0f, 0.0f});
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
                v.worldToScreen(sectorOffset({sx, sy}, sim_.origin()) + glm::vec2(-kSectorHalf, -kSectorHalf));
            std::snprintf(buf, sizeof buf, "%d,%d", sx, sy);
            const bool home = SectorCoord{sx, sy} == sim_.galaxy().home;
            r_.drawText(home ? std::string("HOME ") + buf : std::string(buf), corner + glm::vec2(8.0f, 8.0f),
                        10.0f, colors::scaled(home ? colors::phosphor : colors::amber, 0.7f), 1.0f);
        }
}

void PlayingState::drawDestination(const CameraView& v, double t) {
    if (!sim_.player().hasDestination() || !sim_.playerAlive()) return;
    LineBatch& lines = r_.lines();
    const glm::vec2 from = sim_.player().position();
    const glm::vec2 to = sim_.player().destination();
    const glm::vec2 d = to - from;
    const float len = glm::length(d);

    // Solid path preview with 40% opacity.
    if (len > 1.0f) {
        const glm::vec4 color = colors::scaled(colors::cyan, 0.4f);
        lines.line(from, to, color, 1.5f);
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
    case ObjectKind::Mine: {
        if (!v.isVisible(pos, o.radius * 2.0f)) return;
        const bool blink = std::fmod(ft * 1.5f + static_cast<float>(o.variant) * 0.37f, 1.0f) < 0.25f;
        if (screenR < kDotRadiusPixels) {
            lines.line(pos, pos, blink ? colors::white : colors::red, 3.0f);
            return;
        }
        lines.shape(mineShape_, Transform2D{.position = pos, .rotation = ft * 0.4f, .scale = o.radius},
                    blink ? colors::white : colors::red, 1.5f);
        break;
    }
    case ObjectKind::BlackHole: {
        const float pull = o.radius * 8.0f;
        if (!v.isVisible(pos, pull)) return;
        lines.shape(orbitShape_, Transform2D{.position = pos, .scale = pull},
                    colors::scaled(colors::white, 0.12f), 1.0f);
        if (screenR < kDotRadiusPixels) {
            lines.line(pos, pos, colors::white, 5.0f);
            return;
        }
        lines.shape(holeShape_, Transform2D{.position = pos, .rotation = ft * 0.8f, .scale = o.radius},
                    colors::white, 1.8f);
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
    const glm::vec2 offset = sectorOffset(sector.coord, sim_.origin());
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
    for (std::size_t i = 0; i < sector.objects.size(); ++i)
        if (!sim_.isDestroyed(sector.coord, i)) drawObject(v, sector.objects[i], offset, t);
}

void PlayingState::drawShieldFlash(const CameraView& v, glm::vec2 pos, float radius, const Vitals& vit) {
    constexpr float kFlashSeconds = 0.35f;
    if (vit.sinceHit >= kFlashSeconds || vit.maxShield <= 0.0f || !v.isVisible(pos, radius * 2.0f)) return;
    const float k = 1.0f - vit.sinceHit / kFlashSeconds;
    r_.lines().shape(circleShape_, Transform2D{.position = pos, .scale = radius * 1.5f},
                     colors::scaled(vit.shield > 0.0f ? colors::cyan : colors::red, k), 1.8f);
}

void PlayingState::drawAliens(const CameraView& v, double alpha, double t) {
    const float af = static_cast<float>(alpha);
    for (const AlienShip& a : sim_.aliens()) {
        const Ship& b = a.body;
        const glm::vec2 pos = b.previousPosition() + (b.position() - b.previousPosition()) * af;
        const float heading = b.previousHeading() + wrapAngle(b.heading() - b.previousHeading()) * af;
        const float radius = a.type->radius;

        glm::vec4 color = tintColor(a.type->tint);
        if (a.hostile()) color = colors::red;
        if (a.state == AiState::Hail && std::fmod(t, 0.8) < 0.4) color = colors::amber;
        const float scale = std::max(radius * 1.2f, kShipIconPixels * 0.8f / v.zoom);
        drawShapeLod(r_.lines(), v, alienShapes_[static_cast<std::size_t>(a.type->shape % kAlienShapes)],
                     Transform2D{.position = pos, .rotation = heading, .scale = scale}, scale, color, 2.2f);
        drawShieldFlash(v, pos, radius, a.vitals);
    }
}

void PlayingState::drawProjectilesAndBursts(const CameraView& v, double alpha) {
    LineBatch& lines = r_.lines();
    const float dt = static_cast<float>(alpha * kStep);
    for (const Projectile& p : sim_.projectiles()) {
        const glm::vec2 pos = p.pos + p.vel * dt;
        if (!v.isVisible(pos, 40.0f)) continue;
        const glm::vec4 c = p.owner == 0 ? colors::phosphor : colors::red;
        lines.line(pos, pos - p.vel * 0.025f, c, 2.5f / std::max(v.zoom, 0.5f));
    }
    for (const Burst& b : bursts_) {
        if (!v.isVisible(b.pos, b.size * 1.5f)) continue;
        const float k = b.age / b.life;
        const float r = b.size * (0.25f + 0.75f * k);
        const glm::vec4 c = colors::scaled(colors::amber, 1.0f - k);
        lines.shape(circleShape_, Transform2D{.position = b.pos, .scale = r}, c, 1.8f);
        for (int i = 0; i < 8; ++i) {
            const float a = 2.0f * kPi * static_cast<float>(i) / 8.0f + b.size;
            const glm::vec2 d(std::cos(a), std::sin(a));
            lines.line(b.pos + d * r * 0.5f, b.pos + d * r * (0.8f + 0.5f * k), c, 1.5f);
        }
    }
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
            if (const Sector* s = sim_.streamer().find({sx, sy})) drawSector(v, *s, t);

    drawAliens(v, alpha, t);

    // Selection ring (at least 12 screen points wide so small objects stay findable).
    if (const auto sel = locate(selection_)) {
        const float ringRadius = std::max(sel->radius * 1.35f, 12.0f / v.zoom);
        const bool targeted = alienAt(selection_) && sim_.target() == selection_.alienId;
        lines.shape(circleShape_,
                    Transform2D{.position = sel->pos,
                                .rotation = static_cast<float>(t) * 0.5f,
                                .scale = ringRadius},
                    targeted ? colors::red : colors::white, 1.3f);
    }

    drawDestination(v, t);

    // Engine exhaust: one tail, brightest at the ship and fading toward its far end.
    if (v.zoom > 0.15f) {
        const auto& pts = exhaust_.points();
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            const glm::vec2& a = pts[i].pos;
            const glm::vec2& b = pts[i + 1].pos;
            if (!v.isVisible(a, 10.0f) && !v.isVisible(b, 10.0f)) continue;
            const float fade = 1.0f - pts[i].age / ExhaustTrail::kLife;
            lines.line(a, b, colors::scaled(colors::amber, fade), 2.0f);
        }
    }

    // Ship (interpolated). When zoomed far out it is drawn as an enlarged icon.
    if (sim_.playerAlive()) {
        const Ship& ship = sim_.player();
        const float af = static_cast<float>(alpha);
        const glm::vec2 shipPos =
            ship.previousPosition() + (ship.position() - ship.previousPosition()) * af;
        const float shipHeading =
            ship.previousHeading() + wrapAngle(ship.heading() - ship.previousHeading()) * af;
        lines.shape(shipShape_,
                    Transform2D{.position = shipPos,
                                .rotation = shipHeading,
                                .scale = std::max(kShipScale, kShipIconPixels / v.zoom)},
                    colors::phosphor, 2.5f);
        drawShieldFlash(v, shipPos, kShipScale * 0.6f, sim_.vitals());
    }

    drawProjectilesAndBursts(v, alpha);

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
