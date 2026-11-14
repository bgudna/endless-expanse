#pragma once

#include <glm/glm.hpp>

#include <cmath>

namespace ee {

// World space is Y-DOWN (matches screen/mouse coordinates). Rotation is in radians;
// a positive angle turns clockwise on screen. A shape's "forward" is +X.
struct Transform2D {
    glm::vec2 position{0.0f};
    float rotation = 0.0f;
    float scale = 1.0f;

    glm::vec2 apply(glm::vec2 p) const {
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);
        return position + scale * glm::vec2(c * p.x - s * p.y, s * p.x + c * p.y);
    }

    glm::mat3 matrix() const {
        const float c = std::cos(rotation) * scale;
        const float s = std::sin(rotation) * scale;
        glm::mat3 m(1.0f);
        m[0] = glm::vec3(c, s, 0.0f);
        m[1] = glm::vec3(-s, c, 0.0f);
        m[2] = glm::vec3(position, 1.0f);
        return m;
    }
};

} // namespace ee
