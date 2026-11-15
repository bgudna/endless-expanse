#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <deque>

namespace ee {

struct TrailPoint {
    glm::vec2 pos;
    float age = 0.0f;
};

// Cosmetic engine exhaust: one continuous tail of points sampled behind the ship
// while it thrusts. Points age and drop off the far end, so the tail fades out.
// Simulated at the fixed step; the renderer draws a segment between neighbours.
class ExhaustTrail {
public:
    static constexpr std::size_t kMaxPoints = 200;
    static constexpr float kSpacing = 5.0f;  // world units between samples
    static constexpr float kLife = 2.0f;     // seconds before a point disappears

    // Records `pos` (the exhaust origin) if it is at least kSpacing from the newest point.
    void emit(glm::vec2 pos);
    void update(float dt);
    void clear() { points_.clear(); }
    void shiftOrigin(glm::vec2 delta) {
        for (TrailPoint& p : points_) p.pos += delta;
    }

    // Ordered newest (nearest the ship) to oldest (far end).
    const std::deque<TrailPoint>& points() const { return points_; }

private:
    std::deque<TrailPoint> points_;
};

} // namespace ee
