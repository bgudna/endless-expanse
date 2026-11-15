#include "sim/Simulation.h"

#include "core/MathUtil.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ee {

namespace {
constexpr float kMineTrigger = 45.0f;
constexpr float kMineBlast = 140.0f;
constexpr float kMineDamage = 35.0f;
constexpr float kRestitution = 0.4f;
constexpr float kImpactFreeSpeed = 40.0f; // impacts slower than this do no damage
constexpr float kImpactDamagePerSpeed = 0.25f;
constexpr float kNebulaDrag = 0.6f;       // 1/s
constexpr float kRadiationShieldDrain = 3.0f;
constexpr float kRadiationHullDrain = 1.0f;
constexpr float kHolePullScale = 8.0f;    // gravity reaches this many horizon radii
constexpr float kHolePullAccel = 520.0f;  // at the horizon edge, units/s^2
constexpr float kHoleDamagePerSecond = 30.0f;
constexpr std::size_t kMaxQueuedEvents = 1024;
constexpr float kTradeHullBonus = 30.0f;    // placeholder reward until M8 adds resources
constexpr float kRescueHullBonus = 15.0f;

glm::vec2 unit(glm::vec2 v, glm::vec2 fallback = {1.0f, 0.0f}) {
    const float l = glm::length(v);
    return l > 1e-4f ? v / l : fallback;
}
glm::vec2 fromAngle(float a) { return {std::cos(a), std::sin(a)}; }
} // namespace

Simulation::Simulation(std::uint64_t galaxySeed, std::vector<AlienType> types, DirectorConfig cfg)
    : galaxy_(makeGalaxy(galaxySeed)), streamer_(galaxySeed),
      types_(std::move(types)), director_(galaxySeed, cfg), rng_(galaxySeed ^ 0x51Aull) {
    player_.teleport({0.0f, 0.0f}, -kPi / 2.0f);
    streamer_.update(origin_, origin_, origin_, 0);
}

const AlienShip* Simulation::findAlien(int id) const {
    if (id < 0) return nullptr;
    for (const AlienShip& a : aliens_)
        if (a.id == id) return &a;
    return nullptr;
}

void Simulation::emit(SimEventType type, glm::vec2 pos, float value, std::string text) {
    if (events_.size() >= kMaxQueuedEvents) return;
    events_.push_back({type, pos, value, std::move(text)});
}

SectorCoord Simulation::sectorOf(glm::vec2 p) const {
    return {origin_.x + static_cast<int>(std::floor((p.x + kSectorHalf) / kSectorSize)),
            origin_.y + static_cast<int>(std::floor((p.y + kSectorHalf) / kSectorSize))};
}

const SectorGrid& Simulation::gridFor(SectorCoord c, const Sector& s) {
    auto it = grids_.find(c);
    if (it == grids_.end()) it = grids_.emplace(c, SectorGrid(s)).first;
    return it->second;
}

// ---- Hazards and collisions ---------------------------------------------------------
void Simulation::detonate(SectorCoord c, std::size_t index, glm::vec2 center) {
    if (!destroyed_.insert({c.x, c.y, static_cast<int>(index)}).second) return;
    emit(SimEventType::Explosion, center, kMineBlast);

    if (playerAlive_) {
        const float d = glm::length(player_.position() - center);
        if (d < kMineBlast) {
            vitals_.damage(kMineDamage * (1.0f - 0.5f * d / kMineBlast));
            message("MINE DETONATION");
        }
    }
    for (AlienShip& a : aliens_) {
        const float d = glm::length(a.body.position() - center);
        if (d < kMineBlast) a.vitals.damage(kMineDamage * (1.0f - 0.5f * d / kMineBlast));
    }
}

void Simulation::resolveStatic(Ship& ship, float radius, Vitals& vitals) {
    const SectorCoord c = sectorOf(ship.position());
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const SectorCoord nc{c.x + dx, c.y + dy};
            const Sector* sec = streamer_.find(nc);
            if (!sec) continue;
            const glm::vec2 offset = sectorOffset(nc, origin_);
            glm::vec2 local = ship.position() - offset;
            const float ex = std::max(std::abs(local.x) - kSectorHalf, 0.0f);
            const float ey = std::max(std::abs(local.y) - kSectorHalf, 0.0f);
            if (std::hypot(ex, ey) > radius + kMineTrigger) continue;

            const SectorGrid& grid = gridFor(nc, *sec);
            scratch_.clear();
            grid.query(local, radius + kMineTrigger, scratch_);
            for (const std::uint16_t idx : scratch_) {
                if (isDestroyed(nc, idx)) continue;
                const SectorObject& o = sec->objects[idx];
                const glm::vec2 center = o.pos + offset;
                const glm::vec2 d = ship.position() - center;
                const float dist = glm::length(d);

                if (o.kind == ObjectKind::Mine) {
                    if (dist < o.radius + radius + kMineTrigger) detonate(nc, idx, center);
                    continue;
                }
                if (dist >= o.radius + radius) continue;

                const glm::vec2 n = unit(d);
                ship.nudge(n * (o.radius + radius - dist));
                const float vn = -glm::dot(ship.velocity(), n);
                if (vn > 0.0f) {
                    ship.setVelocity(ship.velocity() + n * vn * (1.0f + kRestitution));
                    float dmg = std::max(0.0f, vn - kImpactFreeSpeed) * kImpactDamagePerSpeed;
                    if (o.kind == ObjectKind::Star) dmg *= 3.0f;
                    if (dmg > 0.0f) {
                        vitals.damage(dmg);
                        emit(SimEventType::Hit, ship.position() - n * radius, dmg);
                    }
                }
            }
        }
}

Simulation::ZoneResult Simulation::applyZones(Ship& ship, Vitals& vitals, float radius, double dtD) {
    const float dt = static_cast<float>(dtD);
    ZoneResult res;
    const SectorCoord c = sectorOf(ship.position());
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const SectorCoord nc{c.x + dx, c.y + dy};
            const Sector* sec = streamer_.find(nc);
            if (!sec) continue;
            const SectorGrid& grid = gridFor(nc, *sec);
            const glm::vec2 offset = sectorOffset(nc, origin_);
            for (const std::uint16_t idx : grid.zones()) {
                const SectorObject& o = sec->objects[idx];
                const glm::vec2 d = ship.position() - (o.pos + offset);
                const float dist = glm::length(d);
                if (o.kind == ObjectKind::Nebula) {
                    if (dist < o.radius * 0.8f) {
                        res.nebula = true;
                        if (o.tint == Tint::Red) res.radiation = true;
                    }
                } else { // black hole
                    const float pullRadius = o.radius * kHolePullScale;
                    if (dist < pullRadius) {
                        const float accel = kHolePullAccel * (1.0f - dist / pullRadius);
                        ship.setVelocity(ship.velocity() - unit(d) * accel * dt);
                    }
                    if (dist < o.radius + radius * 0.5f) vitals.damage(kHoleDamagePerSecond * dt, true);
                }
            }
        }

    if (res.nebula) ship.setVelocity(ship.velocity() * std::exp(-kNebulaDrag * dt));
    if (res.radiation) {
        vitals.sinceHit = 0.0f; // blocks shield recharge
        if (vitals.shield > 0.0f)
            vitals.shield = std::max(0.0f, vitals.shield - kRadiationShieldDrain * dt);
        else
            vitals.damage(kRadiationHullDrain * dt, true);
    }
    return res;
}

// ---- Combat ----------------------------------------------------------------------------
void Simulation::spawnProjectile(glm::vec2 pos, glm::vec2 dir, float speed, float range, float damage,
                                 int owner) {
    projectiles_.push_back({pos, dir * speed, range / std::max(speed, 1.0f), damage, owner});
}

void Simulation::firePlayer(const AlienShip& target) {
    const glm::vec2 pos = player_.position();
    const glm::vec2 toTarget = target.body.position() - pos;
    const float flight = glm::length(toTarget) / weapon_.projectileSpeed;
    const glm::vec2 aim = unit(toTarget + (target.body.velocity() - player_.velocity()) * flight);
    spawnProjectile(pos + aim * (kPlayerRadius + 4.0f), aim, weapon_.projectileSpeed, weapon_.range,
                    weapon_.damage, 0);
    // Projectiles inherit the shooter's velocity so firing while moving feels right.
    projectiles_.back().vel += player_.velocity();
    playerCooldown_ = weapon_.cooldown;
}

bool Simulation::projectileHitsStatic(const Projectile& p) {
    const SectorCoord c = sectorOf(p.pos);
    const Sector* sec = streamer_.find(c);
    if (!sec) return false;
    const glm::vec2 offset = sectorOffset(c, origin_);
    const glm::vec2 local = p.pos - offset;
    scratch_.clear();
    gridFor(c, *sec).query(local, 3.0f, scratch_);
    for (const std::uint16_t idx : scratch_) {
        if (isDestroyed(c, idx)) continue;
        const SectorObject& o = sec->objects[idx];
        if (glm::length(local - o.pos) >= o.radius + 2.0f) continue;
        if (o.kind == ObjectKind::Mine) detonate(c, idx, o.pos + offset);
        return true;
    }
    return false;
}

void Simulation::updateProjectiles(double dtD) {
    const float dt = static_cast<float>(dtD);
    std::size_t keep = 0;
    for (std::size_t i = 0; i < projectiles_.size(); ++i) {
        Projectile p = projectiles_[i];
        p.pos += p.vel * dt;
        p.life -= dt;
        bool consumed = p.life <= 0.0f || projectileHitsStatic(p);

        if (!consumed && p.owner == 0) {
            for (AlienShip& a : aliens_) {
                if (glm::length(a.body.position() - p.pos) < a.type->radius + 3.0f) {
                    a.vitals.damage(p.damage);
                    a.provoked = true;
                    emit(SimEventType::Hit, p.pos, p.damage);
                    consumed = true;
                    break;
                }
            }
        } else if (!consumed && playerAlive_ &&
                   glm::length(player_.position() - p.pos) < kPlayerRadius + 3.0f) {
            vitals_.damage(p.damage);
            emit(SimEventType::Hit, p.pos, p.damage);
            consumed = true;
        }
        if (!consumed) projectiles_[keep++] = p;
    }
    projectiles_.resize(keep);
}

// ---- Aliens ------------------------------------------------------------------------------
void Simulation::answerHail(bool accept) {
    for (AlienShip& a : aliens_)
        if (a.id == hailingId_ && !a.hailAnswered) {
            a.hailAnswered = true;
            a.hailAccepted = accept;
        }
    hailingId_ = -1;
}

void Simulation::updateAliensAndCleanup(double dt) {
    AiContext ctx;
    ctx.playerPos = player_.position();
    ctx.playerVel = player_.velocity();
    ctx.playerAlive = playerAlive_;
    ctx.sightScale = inNebula_ ? 0.5f : 1.0f;

    for (AlienShip& a : aliens_) {
        const AiOutput out = updateAlien(a, ctx, dt);
        if (out.fire) {
            spawnProjectile(a.body.position() + out.aimDir * (a.type->radius + 4.0f), out.aimDir,
                            a.type->projectileSpeed, a.type->range * 1.15f, a.type->damage, a.id);
            projectiles_.back().vel += a.body.velocity() * 0.5f;
        }
        resolveStatic(a.body, a.type->radius, a.vitals);
        applyZones(a.body, a.vitals, a.type->radius, dt);
        a.vitals.update(static_cast<float>(dt));

        if (a.completedTrade) {
            a.completedTrade = false;
            if (a.type->role == Role::Trader) {
                vitals_.repairHull(kTradeHullBonus);
                vitals_.shield = vitals_.maxShield;
                message("TRADE COMPLETE: HULL REPAIRED, SHIELDS RECHARGED");
            } else {
                vitals_.repairHull(kRescueHullBonus);
                message("SURVIVORS SHARE SUPPLIES: HULL REPAIRED");
            }
        }
    }

    // Removal: destroyed, or far away and no longer engaged.
    std::size_t keep = 0;
    for (std::size_t i = 0; i < aliens_.size(); ++i) {
        AlienShip& a = aliens_[i];
        const float dist = glm::length(a.body.position() - player_.position());
        const float limit = director_.config().despawnDistance;
        if (!a.vitals.alive()) {
            emit(SimEventType::Explosion, a.body.position(), a.type->radius * 3.0f);
            emit(SimEventType::AlienDestroyed, a.body.position(), 0.0f, a.type->name);
            ++kills_;
            message(a.type->name + " DESTROYED");
            if (target_ == a.id) target_ = -1;
            if (hailingId_ == a.id) hailingId_ = -1;
            continue;
        }
        const bool engaged = a.hostile();
        if ((dist > limit && (a.leaving || !engaged)) || dist > limit * 1.5f) {
            if (target_ == a.id) target_ = -1;
            if (hailingId_ == a.id) hailingId_ = -1;
            continue;
        }
        if (keep != i) aliens_[keep] = std::move(aliens_[i]);
        ++keep;
    }
    aliens_.erase(aliens_.begin() + static_cast<std::ptrdiff_t>(keep), aliens_.end());

    // Announce a new hail once.
    if (hailingId_ < 0 || !findAlien(hailingId_)) {
        hailingId_ = -1;
        for (const AlienShip& a : aliens_)
            if (a.state == AiState::Hail && !a.hailAnswered) {
                hailingId_ = a.id;
                message(a.type->role == Role::Distress ? "SURVIVORS HAIL: REQUESTING ASSISTANCE"
                                                       : a.type->name + " HAILS YOU: OFFER TRADE");
                break;
            }
    }
}

AlienShip& Simulation::spawnAlien(const AlienType& type, glm::vec2 pos, int encounterId) {
    aliens_.emplace_back(nextAlienId_++, &type, rng_.nextU64());
    AlienShip& a = aliens_.back();
    a.encounterId = encounterId;
    a.body.teleport(pos, rng_.range(-kPi, kPi));
    return a;
}

float Simulation::difficulty() const {
    return 1.0f + std::hypot(static_cast<float>(origin_.x), static_cast<float>(origin_.y)) / 25.0f;
}

const AlienType* Simulation::pickType(Role role, float diff) {
    float total = 0.0f;
    for (const AlienType& t : types_)
        if (t.role == role && t.minDifficulty <= diff) total += t.weight;
    if (total <= 0.0f) return nullptr;
    float pick = rng_.range(0.0f, total);
    const AlienType* last = nullptr;
    for (const AlienType& t : types_) {
        if (t.role != role || t.minDifficulty > diff) continue;
        last = &t;
        if (pick < t.weight) return &t;
        pick -= t.weight;
    }
    return last;
}

bool Simulation::startEncounter(EncounterKind kind) {
    const Role role = [&] {
        switch (kind) {
        case EncounterKind::Hostile:
        case EncounterKind::Ambush: return Role::Raider;
        case EncounterKind::Trade: return Role::Trader;
        case EncounterKind::Distress: return Role::Distress;
        case EncounterKind::PassBy: return Role::Passer;
        }
        return Role::Raider;
    }();
    const float diff = difficulty();
    if (!pickType(role, diff)) return false;

    const DirectorConfig& cfg = director_.config();
    const int room = cfg.maxAliens - static_cast<int>(aliens_.size());
    if (room <= 0) return false;

    const int id = nextEncounterId_++;
    const glm::vec2 pp = player_.position();
    // Ahead of the ship's travel when it is moving, otherwise anywhere.
    const float base = player_.speed() > 60.0f ? std::atan2(player_.velocity().y, player_.velocity().x)
                                               : rng_.range(-kPi, kPi);
    const float angle = base + rng_.range(-1.0f, 1.0f);
    const float dist = rng_.range(cfg.spawnDistMin, cfg.spawnDistMax);
    const glm::vec2 spawn = pp + fromAngle(angle) * dist;

    switch (kind) {
    case EncounterKind::Hostile: {
        const int count = std::min(room, 1 + static_cast<int>(rng_.nextFloat() * (1.0f + (diff - 1.0f) * 2.0f)));
        for (int i = 0; i < std::min(count, 4); ++i) {
            const AlienType* t = pickType(role, diff);
            AlienShip& a = spawnAlien(*t, spawn + fromAngle(rng_.range(0.0f, 2.0f * kPi)) * rng_.range(0.0f, 200.0f), id);
            a.scriptedGoal = true;
            a.body.setDestination(pp);
        }
        message("UNKNOWN CONTACTS APPROACHING");
        break;
    }
    case EncounterKind::Ambush: {
        const int count = std::min(room, diff > 2.0f ? 3 : 2);
        const float start = rng_.range(0.0f, 2.0f * kPi);
        for (int i = 0; i < count; ++i) {
            const AlienType* t = pickType(role, diff);
            const float a = start + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count);
            AlienShip& ship = spawnAlien(*t, pp + fromAngle(a) * rng_.range(700.0f, 900.0f), id);
            ship.setState(AiState::Pursue);
        }
        message("AMBUSH! HOSTILES CLOSING FAST");
        break;
    }
    case EncounterKind::Trade: {
        AlienShip& a = spawnAlien(*pickType(role, diff), spawn, id);
        a.scriptedGoal = true;
        a.body.setDestination(pp);
        message("INCOMING VESSEL, NO WEAPONS LOCK");
        break;
    }
    case EncounterKind::Distress: {
        const glm::vec2 pos = pp + fromAngle(angle) * (dist + 500.0f);
        spawnAlien(*pickType(role, diff), pos, id); // state Idle (set by the constructor)
        char buf[96];
        std::snprintf(buf, sizeof buf, "DISTRESS CALL, SOURCE %.0f UNITS AWAY", static_cast<double>(dist + 500.0f));
        message(buf);
        break;
    }
    case EncounterKind::PassBy: {
        AlienShip& a = spawnAlien(*pickType(role, diff), spawn, id);
        const glm::vec2 toPlayer = pp - spawn;
        const glm::vec2 perp{-toPlayer.y, toPlayer.x};
        a.scriptedGoal = true;
        a.body.setDestination(spawn + toPlayer * 2.2f + unit(perp) * 400.0f);
        message("VESSEL ON A PASSING COURSE");
        break;
    }
    }
    ++encountersStarted_;
    ++perKind_[static_cast<std::size_t>(kind)];
    emit(SimEventType::EncounterStarted, spawn, static_cast<float>(kind), encounterKindName(kind));
    return true;
}

// ---- Origin and main step ----------------------------------------------------------------
void Simulation::rebaseOrigin() {
    const glm::vec2 p = player_.position();
    const int dx = static_cast<int>(std::floor((p.x + kSectorHalf) / kSectorSize));
    const int dy = static_cast<int>(std::floor((p.y + kSectorHalf) / kSectorSize));
    if (dx == 0 && dy == 0) return;
    const glm::vec2 shift(-static_cast<float>(dx) * kSectorSize, -static_cast<float>(dy) * kSectorSize);
    origin_.x += dx;
    origin_.y += dy;
    player_.shiftOrigin(shift);
    for (AlienShip& a : aliens_) a.body.shiftOrigin(shift);
    for (Projectile& pr : projectiles_) pr.pos += shift;
    pendingShift_ += shift;
}

void Simulation::update(double dt) {
    const float fdt = static_cast<float>(dt);
    time_ += dt;
    ++stepCount_;

    if (playerAlive_) {
        player_.update(dt);
        resolveStatic(player_, kPlayerRadius, vitals_);
        const ZoneResult z = applyZones(player_, vitals_, kPlayerRadius, dt);
        inNebula_ = z.nebula;
        inRadiation_ = z.radiation;
        vitals_.update(fdt, !z.nebula);
        if (godMode_) {
            vitals_.hull = vitals_.maxHull;
            vitals_.shield = vitals_.maxShield;
        }
    }

    updateAliensAndCleanup(dt);

    // Player weapon: auto-fires at the targeted ship.
    playerCooldown_ = std::max(0.0f, playerCooldown_ - fdt);
    if (target_ >= 0) {
        const AlienShip* t = findAlien(target_);
        if (!t) {
            target_ = -1;
        } else if (playerAlive_ && playerCooldown_ <= 0.0f &&
                   glm::length(t->body.position() - player_.position()) <= weapon_.range &&
                   (weaponsFree_ || t->hostile() || t->provoked)) {
            firePlayer(*t);
        }
    }
    updateProjectiles(dt);

    if (playerAlive_ && !vitals_.alive()) {
        playerAlive_ = false;
        emit(SimEventType::Explosion, player_.position(), 90.0f);
        emit(SimEventType::PlayerDestroyed, player_.position(), 0.0f);
        message("HULL BREACH: SHIP LOST");
    }

    rebaseOrigin();
    streamer_.update(origin_, lo_, hi_, 4);
    if (stepCount_ % 300 == 0) {
        for (auto it = grids_.begin(); it != grids_.end();)
            it = streamer_.find(it->first) ? std::next(it) : grids_.erase(it);
    }

    if (playerAlive_) {
        EncounterKind kind = EncounterKind::PassBy;
        const bool active = !aliens_.empty();
        if (director_.update(dt, active, kind)) startEncounter(kind);
    }
}

} // namespace ee
