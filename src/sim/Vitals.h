#pragma once

namespace ee {

// Hull and shield for any ship. Shields absorb damage first and recharge after a
// quiet period; hull does not recharge by itself.
struct Vitals {
    float hull = 100.0f;
    float maxHull = 100.0f;
    float shield = 60.0f;
    float maxShield = 60.0f;
    float shieldRegen = 6.0f;   // per second
    float regenDelay = 4.0f;    // seconds without damage before shields recharge
    float sinceHit = 99.0f;     // seconds since the last damage (for effects)

    static Vitals make(float hull, float shield) {
        Vitals v;
        v.hull = v.maxHull = hull;
        v.shield = v.maxShield = shield;
        return v;
    }

    bool alive() const { return hull > 0.0f; }
    float hullFraction() const { return maxHull > 0.0f ? hull / maxHull : 0.0f; }
    float shieldFraction() const { return maxShield > 0.0f ? shield / maxShield : 0.0f; }
    // Combined health 0..1 (used by AI to decide when to flee).
    float strength() const;

    // Applies damage; `piercing` bypasses the shield. Ignores non-positive amounts.
    void damage(float amount, bool piercing = false);
    void repairHull(float amount);
    void update(float dt, bool allowRegen = true);
};

} // namespace ee
