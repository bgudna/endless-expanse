#pragma once

#include "world/Sector.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace ee {

// Broadphase for one sector: a uniform grid over the sector square holding the indices
// of its solid objects, plus a short list of area "zones" (nebulae, black holes).
class SectorGrid {
public:
    static constexpr int kCells = 30;
    static constexpr float kCellSize = kSectorSize / static_cast<float>(kCells);

    explicit SectorGrid(const Sector& sector);

    // Appends (deduplicated) indices of solid objects whose cells overlap the circle.
    // The caller does the exact circle test. `local` is sector-local.
    void query(glm::vec2 local, float radius, std::vector<std::uint16_t>& out) const;

    const std::vector<std::uint16_t>& zones() const { return zones_; }

    static bool isSolid(ObjectKind k);
    static bool isZone(ObjectKind k);

private:
    static int cellIndex(float coord);

    std::vector<std::vector<std::uint16_t>> cells_;
    std::vector<std::uint16_t> zones_;
};

} // namespace ee
