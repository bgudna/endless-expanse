#include "sim/Director.h"

#include <algorithm>

namespace ee {

const char* encounterKindName(EncounterKind k) {
    switch (k) {
    case EncounterKind::Hostile: return "HOSTILE";
    case EncounterKind::Ambush: return "AMBUSH";
    case EncounterKind::Trade: return "TRADE";
    case EncounterKind::Distress: return "DISTRESS";
    case EncounterKind::PassBy: return "PASS-BY";
    }
    return "?";
}

EncounterDirector::EncounterDirector(std::uint64_t seed, DirectorConfig cfg)
    : cfg_(cfg), rng_(seed ^ 0xD1EC70Aull) {
    gap_ = std::max(cfg_.firstDelay, cfg_.minQuiet);
}

float EncounterDirector::rollGap(bool afterCombat) {
    float g = cfg_.avgInterval * rng_.range(1.0f - cfg_.jitter, 1.0f + cfg_.jitter);
    if (afterCombat) g *= cfg_.afterCombatScale;
    return std::max(g, cfg_.minQuiet);
}

EncounterKind EncounterDirector::chooseKind() const {
    // Weights; combat is damped right after a fight so the player gets variety.
    const float combatScale = lastWasCombat_ ? 0.3f : 1.0f;
    const float w[5] = {30.0f * combatScale, 15.0f * combatScale, 20.0f, 15.0f, 20.0f};
    float total = 0.0f;
    for (float x : w) total += x;
    float pick = rng_.range(0.0f, total);
    for (int i = 0; i < 5; ++i) {
        if (pick < w[i]) return static_cast<EncounterKind>(i);
        pick -= w[i];
    }
    return EncounterKind::PassBy;
}

bool EncounterDirector::update(double dt, bool encounterActive, EncounterKind& kind) {
    if (encounterActive) {
        wasActive_ = true;
        return false;
    }
    if (wasActive_) { // the encounter ended since the last call; start the quiet period
        wasActive_ = false;
        elapsed_ = 0.0f;
        gap_ = rollGap(lastWasCombat_);
    }
    elapsed_ += static_cast<float>(dt);
    if (elapsed_ < gap_) return false;

    kind = chooseKind();
    lastKind_ = kind;
    lastWasCombat_ = isCombat(kind);
    wasActive_ = true;
    return true;
}

void EncounterDirector::encounterEnded() {
    wasActive_ = false;
    elapsed_ = 0.0f;
    gap_ = rollGap(lastWasCombat_);
}

float EncounterDirector::tension() const { return gap_ > 0.0f ? std::clamp(elapsed_ / gap_, 0.0f, 1.0f) : 0.0f; }

} // namespace ee
