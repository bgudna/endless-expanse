#pragma once

#include "core/Random.h"

#include <cstdint>

namespace ee {

enum class EncounterKind { Hostile, Ambush, Trade, Distress, PassBy };
const char* encounterKindName(EncounterKind k);
inline bool isCombat(EncounterKind k) { return k == EncounterKind::Hostile || k == EncounterKind::Ambush; }

struct DirectorConfig {
    float firstDelay = 20.0f;    // grace period at the start of the game
    float avgInterval = 75.0f;   // average quiet time between encounters
    float minQuiet = 25.0f;      // hard floor: never sooner than this after one ends
    float jitter = 0.4f;         // gap is avgInterval * [1 - jitter, 1 + jitter]
    float afterCombatScale = 1.3f; // breather after a fight
    int maxAliens = 6;           // alive at once (also stops new encounters)
    float spawnDistMin = 1400.0f;
    float spawnDistMax = 2000.0f;
    float despawnDistance = 6000.0f;
};

// Decides *when* and *what kind* of encounter starts. Pacing: one encounter at a time,
// a guaranteed quiet gap after each, a longer breather after combat, and a kind
// selection that favours peaceful encounters right after a fight.
class EncounterDirector {
public:
    EncounterDirector(std::uint64_t seed, DirectorConfig cfg);

    // Call every step. Returns true (and sets `kind`) when an encounter should start now.
    bool update(double dt, bool encounterActive, EncounterKind& kind);
    // Call when the active encounter has finished (all its ships gone).
    void encounterEnded();

    // 0..1: how close the next encounter is (for debug display).
    float tension() const;
    const DirectorConfig& config() const { return cfg_; }
    Random& rng() { return rng_; }

private:
    float rollGap(bool afterCombat);
    EncounterKind chooseKind() const;

    DirectorConfig cfg_;
    mutable Random rng_;
    float elapsed_ = 0.0f;
    float gap_ = 0.0f;
    bool lastWasCombat_ = false;
    bool wasActive_ = false;
    EncounterKind lastKind_ = EncounterKind::PassBy;
};

} // namespace ee
