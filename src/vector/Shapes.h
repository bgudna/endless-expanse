#pragma once

#include "vector/VectorShape.h"

#include <cstdint>

namespace ee::shapes {

// Procedural placeholder art. All shapes have a unit radius and face +X.
VectorShape ship();
VectorShape circle(int segments = 32);
VectorShape ngon(int sides);
VectorShape asteroid(std::uint64_t seed, int vertices = 10);
VectorShape starCross();
VectorShape planet();

} // namespace ee::shapes
