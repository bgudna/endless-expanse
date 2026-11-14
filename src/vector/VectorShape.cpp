#include "vector/VectorShape.h"

#include <algorithm>

namespace ee {

std::size_t VectorShape::segmentCount() const {
    std::size_t n = 0;
    for (const Polyline& p : paths) n += p.segmentCount();
    return n;
}

float VectorShape::boundingRadius() const {
    float r2 = 0.0f;
    for (const Polyline& p : paths)
        for (const glm::vec2& pt : p.points) r2 = std::max(r2, pt.x * pt.x + pt.y * pt.y);
    return std::sqrt(r2);
}

} // namespace ee
