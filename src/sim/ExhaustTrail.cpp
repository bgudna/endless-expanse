#include "sim/ExhaustTrail.h"

namespace ee {

void ExhaustTrail::emit(glm::vec2 pos, glm::vec2 exhaustDir, glm::vec2 shipVelocity, Random& rng) {
    if (particles_.size() >= kMaxParticles) return;
    const glm::vec2 perp(-exhaustDir.y, exhaustDir.x);
    ExhaustParticle p;
    p.pos = pos;
    p.vel = shipVelocity * 0.5f + exhaustDir * rng.range(90.0f, 180.0f) +
            perp * rng.range(-25.0f, 25.0f);
    p.life = rng.range(0.3f, 0.6f);
    particles_.push_back(p);
}

void ExhaustTrail::update(float dt) {
    for (std::size_t i = 0; i < particles_.size();) {
        ExhaustParticle& p = particles_[i];
        p.age += dt;
        p.pos += p.vel * dt;
        if (p.age >= p.life) {
            p = particles_.back();
            particles_.pop_back();
        } else {
            ++i;
        }
    }
}

} // namespace ee
