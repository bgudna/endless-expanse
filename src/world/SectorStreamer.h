#pragma once

#include "world/Sector.h"

#include <cstdint>
#include <unordered_map>

namespace ee {

// Keeps a window of generated sectors in memory.
//  - Sectors within one step of `center` are always generated immediately (the ship can
//    never see an unloaded neighbour).
//  - Other sectors inside the wanted box are generated nearest-first, at most `budget`
//    per update, so zooming out never causes a single-frame hitch.
//  - Sectors more than one step outside the wanted box are evicted (hysteresis avoids
//    thrashing at a border).
class SectorStreamer {
public:
    explicit SectorStreamer(std::uint64_t galaxySeed) : seed_(galaxySeed) {}

    void update(SectorCoord center, SectorCoord lo, SectorCoord hi, int budget);

    const Sector* find(SectorCoord c) const {
        const auto it = sectors_.find(c);
        return it == sectors_.end() ? nullptr : &it->second;
    }
    std::size_t loadedCount() const { return sectors_.size(); }
    std::size_t loadedObjectCount() const {
        std::size_t n = 0;
        for (const auto& kv : sectors_) n += kv.second.objects.size();
        return n;
    }

private:
    std::uint64_t seed_;
    std::unordered_map<SectorCoord, Sector, SectorCoordHash> sectors_;
};

} // namespace ee
