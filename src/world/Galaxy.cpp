#include "world/Galaxy.h"

#include "core/MathUtil.h"
#include "core/Random.h"

#include <cmath>

namespace ee {

Galaxy makeGalaxy(std::uint64_t seed) {
    Random rng(seed ^ 0xC0FFEE1234ull);
    const float angle = rng.range(0.0f, 2.0f * kPi);
    const float dist = rng.range(30.0f, 45.0f);
    Galaxy g;
    g.seed = seed;
    g.home = {static_cast<int>(std::lround(std::cos(angle) * dist)),
              static_cast<int>(std::lround(std::sin(angle) * dist))};
    return g;
}

HomeHint homeHint(const Galaxy& galaxy, SectorCoord from) {
    static const char* const kNames[8] = {"EAST",  "SOUTHEAST", "SOUTH", "SOUTHWEST",
                                          "WEST",  "NORTHWEST", "NORTH", "NORTHEAST"};
    const double dx = galaxy.home.x - from.x;
    const double dy = galaxy.home.y - from.y; // Y-down: +y is south
    const double dist = std::sqrt(dx * dx + dy * dy);

    HomeHint h;
    if (dist < 0.5) return h;
    const double angle = std::atan2(dy, dx); // 0 = east, clockwise positive
    const long idx = std::lround(angle / (kPi / 4.0)); // -4..4
    h.compass = kNames[((idx % 8) + 8) % 8];
    h.roughSectors = std::max(1, static_cast<int>(std::lround(dist / 5.0)) * 5);
    return h;
}

std::string homeHintText(const HomeHint& hint) {
    if (hint.roughSectors == 0) return "HOME IS IN THIS SECTOR";
    return std::string("HOME ") + hint.compass + ", ~" + std::to_string(hint.roughSectors) + " SECTORS";
}

} // namespace ee
