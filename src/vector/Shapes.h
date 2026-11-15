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
// Placeholder alien hulls: 0 dart, 1 claw, 2 cruiser, 3 freighter, 4 saucer. Face +X, unit radius.
VectorShape alien(int kind);
// Eight-spoked spiky circle used for mines (unit radius).
VectorShape mine();
// Black hole: concentric swirl rings (unit radius).
VectorShape blackHole();

} // namespace ee::shapes
