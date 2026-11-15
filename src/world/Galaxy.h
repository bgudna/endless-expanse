#pragma once

#include "world/Sector.h"

#include <cstdint>
#include <string>

namespace ee {

// The whole galaxy is implicit: a seed plus the sector where home is.
struct Galaxy {
    std::uint64_t seed = 1;
    SectorCoord home;
};

Galaxy makeGalaxy(std::uint64_t seed);

// A deliberately vague pointer toward home: 8-way compass and a distance rounded to 5 sectors.
struct HomeHint {
    const char* compass = "";
    int roughSectors = 0; // 0 means "in this very sector"
};

HomeHint homeHint(const Galaxy& galaxy, SectorCoord from);
std::string homeHintText(const HomeHint& hint);

} // namespace ee
