#include "world/Sector.h"

#include "core/MathUtil.h"
#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace ee {

namespace {
constexpr float kStartKeepOut = 400.0f; // empty disc around the ship's starting position

std::uint64_t mix(std::uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

int randInt(Random& rng, int lo, int hi) { // inclusive
    return lo + static_cast<int>(rng.nextU64() % static_cast<std::uint64_t>(hi - lo + 1));
}

glm::vec2 clampToSector(glm::vec2 p, float margin) {
    const float h = kSectorHalf - margin;
    return {std::clamp(p.x, -h, h), std::clamp(p.y, -h, h)};
}

glm::vec2 unitVec(float angle) { return {std::cos(angle), std::sin(angle)}; }

Tint pickTint(Random& rng, std::initializer_list<Tint> options) {
    return *(options.begin() + static_cast<std::ptrdiff_t>(rng.nextU64() % options.size()));
}

class Builder {
public:
    Builder(std::uint64_t seed, SectorCoord c) : rng(hashSector(seed, c)), isStart(c == SectorCoord{}) {
        s.coord = c;
    }

    // True if a circle at p (radius r) overlaps no solid object and the start keep-out.
    bool clear(glm::vec2 p, float r, float margin) const {
        if (isStart && glm::length(p) < r + kStartKeepOut) return false;
        for (const SectorObject& o : s.objects) {
            if (o.kind == ObjectKind::Rock || o.kind == ObjectKind::Debris ||
                o.kind == ObjectKind::Nebula)
                continue;
            if (glm::length(p - o.pos) < r + o.radius + margin) return false;
        }
        return true;
    }

    void add(ObjectKind kind, glm::vec2 pos, float radius, float spin, int variant, Tint tint) {
        s.objects.push_back({kind, pos, radius, spin, variant, tint});
    }

    void makeStar() {
        if (rng.nextFloat() >= 0.55f) return;
        const float r = rng.range(120.0f, 320.0f);
        glm::vec2 pos;
        bool ok = false;
        for (int attempt = 0; attempt < 8 && !ok; ++attempt) {
            pos = {rng.range(-0.3f, 0.3f) * kSectorSize, rng.range(-0.3f, 0.3f) * kSectorSize};
            ok = clear(pos, r, 0.0f);
        }
        if (!ok) return;
        s.starIndex = static_cast<int>(s.objects.size());
        add(ObjectKind::Star, pos, r, 0.0f, randInt(rng, 0, kShapeVariants - 1),
            pickTint(rng, {Tint::Amber, Tint::Amber, Tint::Red, Tint::White, Tint::Cyan}));

        const int planets = randInt(rng, 0, 4);
        float orbit = r + rng.range(350.0f, 600.0f);
        for (int i = 0; i < planets; ++i) {
            const float pr = rng.range(30.0f, 140.0f);
            const glm::vec2 p = pos + unitVec(rng.range(0.0f, 2.0f * kPi)) * orbit;
            if (std::abs(p.x) < kSectorHalf - pr - 40.0f && std::abs(p.y) < kSectorHalf - pr - 40.0f &&
                clear(p, pr, 40.0f))
                add(ObjectKind::Planet, p, pr, 0.0f, randInt(rng, 0, kShapeVariants - 1),
                    pickTint(rng, {Tint::Cyan, Tint::Phosphor, Tint::Amber, Tint::Red}));
            orbit += pr + rng.range(350.0f, 700.0f);
        }
    }

    void makeNebulae() {
        const int n = randInt(rng, 0, 2);
        for (int i = 0; i < n; ++i) {
            const float r = rng.range(600.0f, 1500.0f);
            const glm::vec2 p{rng.range(-0.4f, 0.4f) * kSectorSize, rng.range(-0.4f, 0.4f) * kSectorSize};
            add(ObjectKind::Nebula, p, r, 0.0f, randInt(rng, 0, kShapeVariants - 1),
                pickTint(rng, {Tint::Cyan, Tint::Red, Tint::Phosphor}));
        }
    }

    void makeAsteroidFields() {
        const int fields = randInt(rng, 0, 2);
        for (int f = 0; f < fields; ++f) {
            const float fr = rng.range(400.0f, 1100.0f);
            const glm::vec2 c{rng.range(-0.4f, 0.4f) * kSectorSize, rng.range(-0.4f, 0.4f) * kSectorSize};
            const int count = randInt(rng, 40, 150);
            for (int i = 0; i < count; ++i) {
                const float rr = rng.range(8.0f, 28.0f);
                const float d = fr * std::sqrt(rng.nextFloat());
                const glm::vec2 p = clampToSector(c + unitVec(rng.range(0.0f, 2.0f * kPi)) * d, rr);
                if (!clear(p, rr, 10.0f)) continue;
                add(ObjectKind::Rock, p, rr, rng.range(-1.0f, 1.0f), randInt(rng, 0, kShapeVariants - 1),
                    Tint::Amber);
            }
        }
    }

    void makeDebris() {
        const int clusters = randInt(rng, 0, 3);
        for (int c = 0; c < clusters; ++c) {
            const glm::vec2 centre{rng.range(-0.45f, 0.45f) * kSectorSize,
                                   rng.range(-0.45f, 0.45f) * kSectorSize};
            const int count = randInt(rng, 4, 10);
            for (int i = 0; i < count; ++i) {
                const float rr = rng.range(3.0f, 7.0f);
                const glm::vec2 p =
                    clampToSector(centre + unitVec(rng.range(0.0f, 2.0f * kPi)) * rng.range(0.0f, 200.0f), rr);
                if (!clear(p, rr, 10.0f)) continue;
                add(ObjectKind::Debris, p, rr, rng.range(-2.0f, 2.0f), randInt(rng, 0, kShapeVariants - 1),
                    Tint::White);
            }
        }
    }

    void makeSolitaries() {
        if (rng.nextFloat() < 0.12f) {
            const glm::vec2 p{rng.range(-0.45f, 0.45f) * kSectorSize, rng.range(-0.45f, 0.45f) * kSectorSize};
            if (clear(p, 20.0f, 100.0f))
                add(ObjectKind::Derelict, p, 20.0f, rng.range(-0.3f, 0.3f),
                    randInt(rng, 0, kShapeVariants - 1), Tint::Red);
        }
        if (rng.nextFloat() < 0.08f) {
            const glm::vec2 p{rng.range(-0.45f, 0.45f) * kSectorSize, rng.range(-0.45f, 0.45f) * kSectorSize};
            if (clear(p, 40.0f, 100.0f))
                add(ObjectKind::Anomaly, p, 40.0f, 0.0f, randInt(rng, 0, kShapeVariants - 1), Tint::Cyan);
        }
    }

    Random rng;
    bool isStart;
    Sector s;
};
} // namespace

std::uint64_t hashSector(std::uint64_t galaxySeed, SectorCoord c) {
    const std::uint64_t packed = static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) |
                                 (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.y)) << 32);
    return mix(galaxySeed ^ mix(packed + 0x9E3779B97F4A7C15ull));
}

Sector generateSector(std::uint64_t galaxySeed, SectorCoord c) {
    Builder b(galaxySeed, c);
    b.makeStar();
    b.makeNebulae();
    b.makeAsteroidFields();
    b.makeDebris();
    b.makeSolitaries();
    return std::move(b.s);
}

const char* objectKindName(ObjectKind kind) {
    switch (kind) {
    case ObjectKind::Star: return "STAR";
    case ObjectKind::Planet: return "PLANET";
    case ObjectKind::Rock: return "ASTEROID";
    case ObjectKind::Debris: return "DEBRIS";
    case ObjectKind::Derelict: return "DERELICT";
    case ObjectKind::Anomaly: return "ANOMALY";
    case ObjectKind::Nebula: return "NEBULA";
    }
    return "OBJECT";
}

} // namespace ee
