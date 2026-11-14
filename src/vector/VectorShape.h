#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace ee {

// One stroke of a vector drawing.
struct Polyline {
    std::vector<glm::vec2> points;
    bool closed = false;
    float intensity = 1.0f; // multiplies the draw colour (for dim detail lines)

    std::size_t segmentCount() const {
        if (points.size() < 2) return 0;
        return closed ? points.size() : points.size() - 1;
    }
};

// A vector drawing in local space (origin at its centre unless stated otherwise).
// This is the stable interface that hand-drawn, imported art will fill later.
struct VectorShape {
    std::vector<Polyline> paths;

    void add(Polyline p) { paths.push_back(std::move(p)); }
    std::size_t segmentCount() const;
    // Largest distance from the local origin to any point.
    float boundingRadius() const;
};

} // namespace ee
