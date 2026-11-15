#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ee {

// Integer grid coordinate of a sector. (0, 0) is the start sector. Y-down like world space.
struct SectorCoord {
    int x = 0;
    int y = 0;
    bool operator==(const SectorCoord&) const = default;
};

struct SectorCoordHash {
    std::size_t operator()(const SectorCoord& c) const {
        return std::hash<std::uint64_t>()(static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) |
                                          (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.y)) << 32));
    }
};

// Sectors are squares of this many world units, centred on their own local origin.
// Every object position is local to its sector: x and y in [-size/2, size/2].
constexpr float kSectorSize = 6000.0f;
constexpr float kSectorHalf = kSectorSize * 0.5f;

enum class ObjectKind { Star, Planet, Rock, Debris, Derelict, Anomaly, Nebula };

// Palette slot; the renderer maps it to an actual colour (keeps this module GL/colour free).
enum class Tint { Cyan, Amber, Phosphor, Red, White };

struct SectorObject {
    ObjectKind kind = ObjectKind::Rock;
    glm::vec2 pos{0.0f}; // sector-local
    float radius = 10.0f;
    float spin = 0.0f;   // rad/s (rocks, debris, derelicts)
    int variant = 0;     // shape variant / random phase, 0..7
    Tint tint = Tint::Amber;
};

struct Sector {
    SectorCoord coord;
    std::vector<SectorObject> objects;
    int starIndex = -1; // index into `objects`, or -1 for deep space
};

constexpr int kShapeVariants = 8;

// Deterministic: the same (galaxySeed, coord) always yields the same sector.
std::uint64_t hashSector(std::uint64_t galaxySeed, SectorCoord coord);
Sector generateSector(std::uint64_t galaxySeed, SectorCoord coord);

// World-space (origin-relative) offset of sector `c` when `origin` is the sector at local (0, 0).
inline glm::vec2 sectorOffset(SectorCoord c, SectorCoord origin) {
    return {static_cast<float>(c.x - origin.x) * kSectorSize,
            static_cast<float>(c.y - origin.y) * kSectorSize};
}

const char* objectKindName(ObjectKind kind);

} // namespace ee
