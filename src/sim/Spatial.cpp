#include "sim/Spatial.h"

#include <algorithm>
#include <cmath>

namespace ee {

bool SectorGrid::isSolid(ObjectKind k) {
    switch (k) {
    case ObjectKind::Star:
    case ObjectKind::Planet:
    case ObjectKind::Rock:
    case ObjectKind::Derelict:
    case ObjectKind::Mine: return true;
    default: return false;
    }
}

bool SectorGrid::isZone(ObjectKind k) { return k == ObjectKind::Nebula || k == ObjectKind::BlackHole; }

int SectorGrid::cellIndex(float coord) {
    const int i = static_cast<int>(std::floor((coord + kSectorHalf) / kCellSize));
    return std::clamp(i, 0, kCells - 1);
}

SectorGrid::SectorGrid(const Sector& sector) : cells_(static_cast<std::size_t>(kCells * kCells)) {
    for (std::size_t i = 0; i < sector.objects.size(); ++i) {
        const SectorObject& o = sector.objects[i];
        const auto idx = static_cast<std::uint16_t>(i);
        if (isZone(o.kind)) zones_.push_back(idx);
        if (!isSolid(o.kind)) continue;
        const int x0 = cellIndex(o.pos.x - o.radius), x1 = cellIndex(o.pos.x + o.radius);
        const int y0 = cellIndex(o.pos.y - o.radius), y1 = cellIndex(o.pos.y + o.radius);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) cells_[static_cast<std::size_t>(y * kCells + x)].push_back(idx);
    }
}

void SectorGrid::query(glm::vec2 local, float radius, std::vector<std::uint16_t>& out) const {
    // Outside the sector square the clamp makes this a query of the border cells, which is
    // harmless: the caller's exact test rejects anything that is not really touching.
    const int x0 = cellIndex(local.x - radius), x1 = cellIndex(local.x + radius);
    const int y0 = cellIndex(local.y - radius), y1 = cellIndex(local.y + radius);
    const std::size_t start = out.size();
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const auto& cell = cells_[static_cast<std::size_t>(y * kCells + x)];
            out.insert(out.end(), cell.begin(), cell.end());
        }
    std::sort(out.begin() + static_cast<std::ptrdiff_t>(start), out.end());
    out.erase(std::unique(out.begin() + static_cast<std::ptrdiff_t>(start), out.end()), out.end());
}

} // namespace ee
