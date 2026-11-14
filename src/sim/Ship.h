#pragma once

#include <glm/glm.hpp>

namespace ee {

struct ShipParams {
    float maxSpeed = 450.0f;     // world units / s
    float thrust = 260.0f;       // forward acceleration, units / s^2
    float turnRate = 3.2f;       // rad / s
    float assistDrag = 0.8f;     // 1/s velocity damping when idle with drag assist on
    float arriveRadius = 4.0f;   // autopilot finishes inside this radius...
    float arriveSpeed = 30.0f;   // ...and below this speed
    float brakeFraction = 0.55f; // autopilot plans to brake at this share of full thrust
    float deadzone = 4.0f;       // ignore velocity errors smaller than this (stops flip-flopping)
    float throttleGain = 3.0f;   // velocity error -> acceleration request
};

// Newtonian-lite starship: it has inertia, turns at a limited rate and can only
// thrust forward, so stopping means turning around and braking. An autopilot flies
// it to a destination (turn, accelerate, coast, flip, brake). Optional drag assist
// bleeds off speed when no destination is set.
//
// No SDL/GL: fully deterministic and unit-testable.
class Ship {
public:
    explicit Ship(ShipParams params = {}) : p_(params) {}

    void update(double dt);

    void setDestination(glm::vec2 world) {
        dest_ = world;
        hasDest_ = true;
    }
    void clearDestination() { hasDest_ = false; }
    bool hasDestination() const { return hasDest_; }
    glm::vec2 destination() const { return dest_; }
    float distanceToDestination() const { return hasDest_ ? glm::length(dest_ - pos_) : 0.0f; }

    void setDragAssist(bool on) { assist_ = on; }
    bool dragAssist() const { return assist_; }

    void teleport(glm::vec2 position, float heading);
    void setVelocity(glm::vec2 v) { vel_ = v; }

    glm::vec2 position() const { return pos_; }
    glm::vec2 velocity() const { return vel_; }
    float speed() const { return glm::length(vel_); }
    float heading() const { return heading_; }
    bool thrusting() const { return thrusting_; }
    glm::vec2 forward() const { return {std::cos(heading_), std::sin(heading_)}; }

    // Previous-step state, for render interpolation.
    glm::vec2 previousPosition() const { return prevPos_; }
    float previousHeading() const { return prevHeading_; }

    const ShipParams& params() const { return p_; }

private:
    ShipParams p_;
    glm::vec2 pos_{0.0f}, vel_{0.0f}, prevPos_{0.0f}, dest_{0.0f};
    float heading_ = 0.0f, prevHeading_ = 0.0f;
    bool hasDest_ = false;
    bool assist_ = true;
    bool thrusting_ = false;
};

} // namespace ee
