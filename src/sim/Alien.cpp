#include "sim/Alien.h"

#include "core/MathUtil.h"

#include <algorithm>
#include <cmath>

namespace ee {

namespace {
constexpr float kHailRangeTrader = 700.0f;
constexpr float kHailRangeDistress = 500.0f;
constexpr float kHailTimeout = 25.0f;
constexpr float kTradeDuration = 5.0f;
constexpr float kFleeStrength = 0.25f;
constexpr float kFleeMaxTime = 25.0f;
constexpr float kFarGoal = 6000.0f;

ShipParams paramsFor(const AlienType& t) {
    ShipParams p;
    p.maxSpeed = t.maxSpeed;
    p.thrust = t.thrust;
    p.turnRate = t.turnRate;
    return p;
}

glm::vec2 unit(glm::vec2 v, glm::vec2 fallback = {1.0f, 0.0f}) {
    const float l = glm::length(v);
    return l > 1e-4f ? v / l : fallback;
}

glm::vec2 rotate(glm::vec2 v, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return {c * v.x - s * v.y, s * v.x + c * v.y};
}

void startLeaving(AlienShip& a, const AiContext& ctx) {
    a.leaving = true;
    a.scriptedGoal = false;
    const glm::vec2 away = unit(a.body.position() - ctx.playerPos, {1.0f, 0.0f});
    a.body.setDestination(a.body.position() + away * kFarGoal);
    a.setState(AiState::Patrol);
}
} // namespace

const char* aiStateName(AiState s) {
    switch (s) {
    case AiState::Idle: return "IDLE";
    case AiState::Patrol: return "PATROL";
    case AiState::Hail: return "HAILING";
    case AiState::Flee: return "FLEEING";
    case AiState::Pursue: return "PURSUING";
    case AiState::Attack: return "ATTACKING";
    case AiState::Trade: return "TRADING";
    }
    return "?";
}

AlienShip::AlienShip(int id_, const AlienType* type_, std::uint64_t seed)
    : id(id_), type(type_), body(paramsFor(*type_)),
      vitals(Vitals::make(type_->hull, type_->shield)), rng(seed) {
    orbitPhase = rng.range(0.0f, 2.0f * kPi);
    state = type->role == Role::Distress ? AiState::Idle : AiState::Patrol;
}

AiOutput updateAlien(AlienShip& a, const AiContext& ctx, double dtD) {
    const float dt = static_cast<float>(dtD);
    AiOutput out;
    const AlienType& t = *a.type;
    a.stateTime += dt;
    a.fireCooldown = std::max(0.0f, a.fireCooldown - dt);
    a.retargetTimer -= dt;

    const glm::vec2 pos = a.body.position();
    const glm::vec2 toPlayer = ctx.playerPos - pos;
    const float dist = glm::length(toPlayer);
    const float sight = t.sight * ctx.sightScale;
    const bool sees = ctx.playerAlive && dist < sight;

    // ---- Transitions that apply in several states ----
    const bool calm = a.state == AiState::Idle || a.state == AiState::Patrol ||
                      a.state == AiState::Hail || a.state == AiState::Trade;
    if (a.provoked && calm) {
        const bool fights = a.armed() && (t.role == Role::Raider || t.role == Role::Passer);
        a.setState(fights ? AiState::Pursue : AiState::Flee);
    }
    if (a.state != AiState::Flee && !a.hasFled && t.role != Role::Distress &&
        a.vitals.strength() < kFleeStrength) {
        a.hasFled = true;
        a.setState(AiState::Flee);
    }

    switch (a.state) {
    case AiState::Idle:
    case AiState::Patrol: {
        const bool canHail = (t.role == Role::Trader || t.role == Role::Distress) && !a.resolved &&
                             !a.provoked && ctx.playerAlive;
        const float hailRange = t.role == Role::Trader ? kHailRangeTrader : kHailRangeDistress;
        if (canHail && dist < hailRange) {
            a.body.clearDestination();
            a.setState(AiState::Hail);
            break;
        }
        if (a.state == AiState::Idle) break; // distress ships just drift
        if (t.role == Role::Raider && sees && !a.leaving) {
            a.setState(AiState::Pursue);
            break;
        }
        if (!a.body.hasDestination()) {
            if (a.scriptedGoal) {
                startLeaving(a, ctx); // finished the pass-by; keep going
            } else if (a.leaving) {
                startLeaving(a, ctx);
            } else {
                const glm::vec2 dir = rotate({1.0f, 0.0f}, a.rng.range(0.0f, 2.0f * kPi));
                a.body.setDestination(pos + dir * a.rng.range(600.0f, 1400.0f));
            }
        }
        break;
    }
    case AiState::Hail:
        a.body.clearDestination();
        if (a.hailAnswered) {
            if (a.hailAccepted) {
                a.setState(AiState::Trade);
            } else {
                a.resolved = true;
                startLeaving(a, ctx);
            }
        } else if (a.stateTime > kHailTimeout || !ctx.playerAlive) {
            a.resolved = true;
            startLeaving(a, ctx);
        }
        break;
    case AiState::Trade:
        a.body.clearDestination();
        if (a.stateTime > kTradeDuration) {
            a.completedTrade = true;
            a.resolved = true;
            startLeaving(a, ctx);
        }
        break;
    case AiState::Pursue:
        if (!ctx.playerAlive || dist > sight * 1.6f) {
            a.setState(AiState::Patrol);
            a.body.clearDestination();
            break;
        }
        a.body.setDestination(ctx.playerPos);
        if (dist < t.range * 0.85f) a.setState(AiState::Attack);
        break;
    case AiState::Attack: {
        if (!ctx.playerAlive) {
            a.setState(AiState::Patrol);
            break;
        }
        if (dist > t.range * 1.2f) {
            a.setState(AiState::Pursue);
            break;
        }
        // Circle the player at about 60% of weapon range instead of parking.
        if (a.retargetTimer <= 0.0f) {
            a.retargetTimer = 0.25f;
            const float swing = 0.9f * std::sin(a.stateTime * 0.5f + a.orbitPhase);
            const glm::vec2 radial = unit(pos - ctx.playerPos);
            a.body.setDestination(ctx.playerPos + rotate(radial, swing) * (t.range * 0.6f));
        }
        if (dist < t.range && a.fireCooldown <= 0.0f && a.armed()) {
            a.fireCooldown = t.cooldown;
            const float flight = dist / std::max(t.projectileSpeed, 1.0f);
            const glm::vec2 lead = ctx.playerPos + ctx.playerVel * flight - pos;
            out.fire = true;
            out.aimDir = rotate(unit(lead), a.rng.range(-t.spread, t.spread));
        }
        break;
    }
    case AiState::Flee:
        if (a.retargetTimer <= 0.0f) {
            a.retargetTimer = 0.5f;
            a.body.setDestination(pos + unit(pos - ctx.playerPos) * 3000.0f);
        }
        if (!ctx.playerAlive || dist > sight * 1.8f || a.stateTime > kFleeMaxTime) startLeaving(a, ctx);
        break;
    }

    a.body.update(dtD);
    return out;
}

} // namespace ee
