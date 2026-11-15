#include "world/SectorStreamer.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace ee {

namespace {
int chebyshev(SectorCoord a, SectorCoord b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }
} // namespace

void SectorStreamer::update(SectorCoord center, SectorCoord lo, SectorCoord hi, int budget) {
    // The wanted box always contains the centre's 3x3 neighbourhood.
    lo = {std::min(lo.x, center.x - 1), std::min(lo.y, center.y - 1)};
    hi = {std::max(hi.x, center.x + 1), std::max(hi.y, center.y + 1)};

    std::vector<SectorCoord> missing;
    for (int y = lo.y; y <= hi.y; ++y)
        for (int x = lo.x; x <= hi.x; ++x) {
            const SectorCoord c{x, y};
            if (sectors_.find(c) != sectors_.end()) continue;
            if (chebyshev(c, center) <= 1)
                sectors_.emplace(c, generateSector(seed_, c));
            else
                missing.push_back(c);
        }

    std::sort(missing.begin(), missing.end(), [&](SectorCoord a, SectorCoord b) {
        return chebyshev(a, center) < chebyshev(b, center);
    });
    for (std::size_t i = 0; i < missing.size() && static_cast<int>(i) < budget; ++i)
        sectors_.emplace(missing[i], generateSector(seed_, missing[i]));

    for (auto it = sectors_.begin(); it != sectors_.end();) {
        const SectorCoord c = it->first;
        const bool keep = c.x >= lo.x - 1 && c.x <= hi.x + 1 && c.y >= lo.y - 1 && c.y <= hi.y + 1;
        it = keep ? std::next(it) : sectors_.erase(it);
    }
}

} // namespace ee
