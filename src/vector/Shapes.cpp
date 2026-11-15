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

VectorShape alien(int kind) {
    VectorShape s;
    auto closed = [&](std::initializer_list<glm::vec2> pts, float intensity = 1.0f) {
        Polyline p;
        p.closed = true;
        p.points = pts;
        p.intensity = intensity;
        s.add(std::move(p));
    };
    auto open = [&](std::initializer_list<glm::vec2> pts, float intensity) {
        Polyline p;
        p.points = pts;
        p.intensity = intensity;
        s.add(std::move(p));
    };
    switch (kind) {
    case 0: // dart: a thin forward-swept arrowhead
        closed({{1.0f, 0.0f}, {-0.6f, 0.45f}, {-0.2f, 0.0f}, {-0.6f, -0.45f}});
        open({{-0.2f, 0.0f}, {0.5f, 0.0f}}, 0.5f);
        break;
    case 1: // claw: two forward pincers around a core
        closed({{0.9f, 0.7f}, {0.2f, 0.35f}, {-0.7f, 0.5f}, {-0.7f, -0.5f}, {0.2f, -0.35f}, {0.9f, -0.7f},
                {0.35f, 0.0f}});
        open({{-0.7f, 0.0f}, {0.2f, 0.0f}}, 0.5f);
        break;
    case 2: // cruiser: long hull with fins and a bridge
        closed({{1.0f, 0.0f}, {0.5f, 0.25f}, {-0.4f, 0.3f}, {-0.9f, 0.8f}, {-0.7f, 0.0f}, {-0.9f, -0.8f},
                {-0.4f, -0.3f}, {0.5f, -0.25f}});
        open({{-0.4f, 0.0f}, {0.6f, 0.0f}}, 0.5f);
        open({{0.1f, 0.18f}, {0.1f, -0.18f}}, 0.5f);
        break;
    case 3: // freighter: boxy with cargo bays
        closed({{0.8f, 0.35f}, {0.8f, -0.35f}, {-0.9f, -0.5f}, {-0.9f, 0.5f}});
        open({{0.2f, 0.4f}, {0.2f, -0.4f}}, 0.5f);
        open({{-0.35f, 0.45f}, {-0.35f, -0.45f}}, 0.5f);
        break;
    default: { // saucer: disc with a dome
        s.add(ellipse(24, 1.0f, 0.55f, 0.0f));
        Polyline dome = ellipse(16, 0.4f, 0.25f, 0.0f);
        dome.intensity = 0.6f;
        s.add(std::move(dome));
        break;
    }
    }
    return s;
}

VectorShape mine() {
    VectorShape s;
    s.add(ellipse(10, 0.55f, 0.55f, 0.0f));
    for (int i = 0; i < 8; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / 8.0f;
        Polyline spike;
        spike.points = {{0.55f * std::cos(a), 0.55f * std::sin(a)}, {std::cos(a), std::sin(a)}};
        s.add(std::move(spike));
    }
    return s;
}

VectorShape blackHole() {
    VectorShape s;
    s.add(ellipse(32, 1.0f, 1.0f, 0.0f));
    for (int k = 1; k <= 3; ++k) {
        // A broken spiral arm per ring gives a swirling look when the shape rotates.
        Polyline arm;
        const float r0 = 1.0f - 0.25f * static_cast<float>(k);
        for (int i = 0; i <= 18; ++i) {
            const float a = 3.5f * static_cast<float>(i) / 18.0f + static_cast<float>(k);
            const float r = r0 * (0.6f + 0.4f * static_cast<float>(i) / 18.0f);
            arm.points.emplace_back(r * std::cos(a), r * std::sin(a));
        }
        arm.intensity = 0.7f;
        s.add(std::move(arm));
    }
    return s;
}

} // namespace ee::shapes
