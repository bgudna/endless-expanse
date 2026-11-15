#pragma once

#include "core/Random.h"
#include "sim/AlienType.h"
#include "sim/Ship.h"
#include "sim/Vitals.h"

#include <glm/glm.hpp>

namespace ee {

enum class AiState { Idle, Patrol, Hail, Flee, Pursue, Attack, Trade };
const char* aiStateName(AiState s);

struct AlienShip {
    AlienShip(int id_, const AlienType* type_, std::uint64_t seed);

    int id;
    const AlienType* type;
    Ship body;
    Vitals vitals;
    AiState state = AiState::Patrol;
    float stateTime = 0.0f;
    float fireCooldown = 0.0f;
    float orbitPhase = 0.0f;
    float retargetTimer = 0.0f;
    int encounterId = -1;

    bool provoked = false;     // the player attacked it
    bool hasFled = false;      // flees at most once
    bool leaving = false;      // done here: heads away and may be despawned
    bool scriptedGoal = false; // patrol target was set by the director (pass-by / approach)
    bool resolved = false;     // hail/trade finished
    bool hailAnswered = false;
    bool hailAccepted = false;
    bool completedTrade = false; // set once; the simulation pays out and clears it
    Random rng;

    bool hostile() const { return state == AiState::Pursue || state == AiState::Attack; }
    bool armed() const { return type->damage > 0.0f; }
    void setState(AiState s) {
        state = s;
        stateTime = 0.0f;
    }
};

struct AiContext {
    glm::vec2 playerPos{0.0f};
    glm::vec2 playerVel{0.0f};
    bool playerAlive = true;
    float sightScale = 1.0f; // < 1 when the player is hidden in a nebula
};

struct AiOutput {
    bool fire = false;
    glm::vec2 aimDir{1.0f, 0.0f}; // unit vector (already includes lead and spread)
};

// Advances one alien's behaviour state machine and its flight by one fixed step.
AiOutput updateAlien(AlienShip& a, const AiContext& ctx, double dt);

} // namespace ee
