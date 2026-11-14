#pragma once

#include "core/Random.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace ee {

struct ExhaustParticle {
    glm::vec2 pos;
    glm::vec2 vel;
    float age = 0.0f;
    float life = 0.5f;
};

// Cosmetic engine exhaust. Simulated at the fixed step; the renderer draws each
// particle as a short fading streak.
class ExhaustTrail {
public:
    static constexpr std::size_t kMaxParticles = 512;

    // `exhaustDir` is the unit direction the exhaust travels (away from the ship).
    void emit(glm::vec2 pos, glm::vec2 exhaustDir, glm::vec2 shipVelocity, Random& rng);
    void update(float dt);
    void clear() { particles_.clear(); }

    const std::vector<ExhaustParticle>& particles() const { return particles_; }

private:
    std::vector<ExhaustParticle> particles_;
};

} // namespace ee
