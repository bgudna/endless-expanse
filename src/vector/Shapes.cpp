#include "vector/Shapes.h"

#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace ee::shapes {

namespace {
constexpr float kPi = 3.14159265358979f;

Polyline ellipse(int segments, float rx, float ry, float tilt) {
    Polyline p;
    p.closed = true;
    const float c = std::cos(tilt);
    const float s = std::sin(tilt);
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = rx * std::cos(a);
        const float y = ry * std::sin(a);
        p.points.emplace_back(c * x - s * y, s * x + c * y);
    }
    return p;
}
} // namespace

VectorShape ship() {
    VectorShape s;
    Polyline hull;
    hull.closed = true;
    hull.points = {{1.0f, 0.0f}, {-0.8f, 0.6f}, {-0.4f, 0.0f}, {-0.8f, -0.6f}};
    s.add(std::move(hull));

    Polyline spine;
    spine.points = {{-0.4f, 0.0f}, {0.3f, 0.0f}};
    spine.intensity = 0.5f;
    s.add(std::move(spine));
    return s;
}

VectorShape circle(int segments) {
    VectorShape s;
    s.add(ellipse(std::max(segments, 3), 1.0f, 1.0f, 0.0f));
    return s;
}

VectorShape ngon(int sides) { return circle(sides); }

VectorShape asteroid(std::uint64_t seed, int vertices) {
    Random rng(seed);
    vertices = std::max(vertices, 5);
    Polyline p;
    p.closed = true;
    for (int i = 0; i < vertices; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(vertices);
        const float r = rng.range(0.65f, 1.0f);
        p.points.emplace_back(r * std::cos(a), r * std::sin(a));
    }
    VectorShape s;
    s.add(std::move(p));
    return s;
}

VectorShape starCross() {
    VectorShape s;
    Polyline h;
    h.points = {{-1.0f, 0.0f}, {1.0f, 0.0f}};
    Polyline v;
    v.points = {{0.0f, -1.0f}, {0.0f, 1.0f}};
    s.add(std::move(h));
    s.add(std::move(v));
    return s;
}

VectorShape planet() {
    VectorShape s;
    s.add(ellipse(48, 1.0f, 1.0f, 0.0f));
    Polyline ring = ellipse(48, 1.0f, 0.25f, -0.35f);
    ring.intensity = 0.6f;
    s.add(std::move(ring));
    return s;
}

} // namespace ee::shapes
