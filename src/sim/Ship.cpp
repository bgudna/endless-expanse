#include "sim/Ship.h"

#include "core/MathUtil.h"

#include <algorithm>
#include <cmath>

namespace ee {

void Ship::teleport(glm::vec2 position, float heading) {
    pos_ = prevPos_ = position;
    heading_ = prevHeading_ = wrapAngle(heading);
    vel_ = glm::vec2(0.0f);
    hasDest_ = false;
}

void Ship::update(double dtSeconds) {
    const float dt = static_cast<float>(dtSeconds);
    prevPos_ = pos_;
    prevHeading_ = heading_;
    thrusting_ = false;

    float desiredAngle = heading_;
    float throttle = 0.0f;
    bool steer = false;

    if (hasDest_) {
        const glm::vec2 toDest = dest_ - pos_;
        const float dist = glm::length(toDest);
        const float speed = glm::length(vel_);

        if (dist <= p_.arriveRadius && speed <= p_.arriveSpeed) {
            pos_ = dest_;
            vel_ = glm::vec2(0.0f);
            hasDest_ = false;
        } else {
            const glm::vec2 dir = dist > 1e-4f ? toDest / dist : glm::vec2(0.0f);

            // Speed profile: sqrt(2*a*d) lets us stop exactly at the target while braking at
            // `aBrake`; inside `crossover` it becomes linear so the final approach is smooth.
            // The turn-around time is subtracted from the available distance.
            const float aBrake = p_.thrust * p_.brakeFraction;
            const float turnLag = speed * (0.75f * kPi / p_.turnRate);
            const float effective = std::max(0.0f, dist - turnLag);
            constexpr float kCrossover = 20.0f;
            const float k = std::sqrt(2.0f * aBrake / kCrossover);
            const float profile = effective < kCrossover ? k * effective
                                                         : std::sqrt(2.0f * aBrake * effective);
            const glm::vec2 desiredVel = dir * std::min(profile, p_.maxSpeed);

            const glm::vec2 dv = desiredVel - vel_;
            const float dvLen = glm::length(dv);
            if (dvLen > p_.deadzone) {
                desiredAngle = std::atan2(dv.y, dv.x);
                throttle = std::clamp(dvLen * p_.throttleGain / p_.thrust, 0.0f, 1.0f);
                steer = true;
            }
        }
    } else if (assist_) {
        vel_ *= std::exp(-p_.assistDrag * dt);
        if (glm::length(vel_) < 0.5f) vel_ = glm::vec2(0.0f);
    }

    if (steer) {
        const float maxTurn = p_.turnRate * dt;
        heading_ = wrapAngle(heading_ + std::clamp(wrapAngle(desiredAngle - heading_), -maxTurn, maxTurn));

        // Thrust only once roughly pointing the right way (full at 0 deg, none beyond 60 deg).
        const float alignment =
            std::clamp((std::cos(wrapAngle(desiredAngle - heading_)) - 0.5f) / 0.5f, 0.0f, 1.0f);
        const float accel = p_.thrust * throttle * alignment;
        if (accel > 0.01f) {
            vel_ += forward() * accel * dt;
            thrusting_ = true;
        }
    }

    const float speed = glm::length(vel_);
    if (speed > p_.maxSpeed) vel_ *= p_.maxSpeed / speed;
    pos_ += vel_ * dt;
}

} // namespace ee
