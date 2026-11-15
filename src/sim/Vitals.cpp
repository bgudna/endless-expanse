#include "sim/Vitals.h"

#include <algorithm>

namespace ee {

float Vitals::strength() const {
    const float total = maxHull + maxShield;
    return total > 0.0f ? (std::max(hull, 0.0f) + shield) / total : 0.0f;
}

void Vitals::damage(float amount, bool piercing) {
    if (amount <= 0.0f || !alive()) return;
    sinceHit = 0.0f;
    if (!piercing && shield > 0.0f) {
        const float absorbed = std::min(shield, amount);
        shield -= absorbed;
        amount -= absorbed;
    }
    hull = std::max(0.0f, hull - amount);
}

void Vitals::repairHull(float amount) { hull = std::clamp(hull + amount, 0.0f, maxHull); }

void Vitals::update(float dt, bool allowRegen) {
    sinceHit += dt;
    if (allowRegen && alive() && sinceHit >= regenDelay)
        shield = std::min(maxShield, shield + shieldRegen * dt);
}

} // namespace ee
