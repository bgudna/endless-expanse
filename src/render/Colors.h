#pragma once

#include <glm/glm.hpp>

namespace ee::colors {

// Retro vector-monitor palette. Values may exceed 1.0 later for extra glow.
inline const glm::vec4 phosphor{0.25f, 1.0f, 0.45f, 1.0f};
inline const glm::vec4 cyan{0.30f, 0.90f, 1.0f, 1.0f};
inline const glm::vec4 amber{1.0f, 0.70f, 0.20f, 1.0f};
inline const glm::vec4 red{1.0f, 0.28f, 0.22f, 1.0f};
inline const glm::vec4 white{0.9f, 0.95f, 1.0f, 1.0f};

inline glm::vec4 scaled(const glm::vec4& c, float f) { return {c.r * f, c.g * f, c.b * f, c.a}; }

} // namespace ee::colors
