#pragma once

#include <cmath>

namespace ee {

inline constexpr float kPi = 3.14159265358979f;

// Wraps an angle to [-pi, pi].
inline float wrapAngle(float a) { return std::remainder(a, 2.0f * kPi); }

} // namespace ee
