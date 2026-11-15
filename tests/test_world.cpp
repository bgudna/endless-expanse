#include <doctest/doctest.h>

#include "camera/Camera2D.h"
#include "sim/Ship.h"
#include "world/Galaxy.h"
#include "world/Sector.h"
#include "world/SectorStreamer.h"

#include <cmath>

using namespace ee;

namespace {
bool sameSector(const Sector& a, const Sector& b) {
    if (a.objects.size() != b.objects.size() || a.starIndex != b.starIndex) return false;
    for (std::size_t i = 0; i < a.objects.size(); ++i) {
        const SectorObject &p = a.objects[i], &q = b.objects[i];
        if (p.kind != q.kind || p.pos != q.pos || p.radius != q.radius || p.spin != q.spin ||
            p.variant != q.variant || p.tint != q.tint)
            return false;
    }
    return true;
}
} // namespace

TEST_CASE("sector generation is deterministic") {
    for (int y = -3; y <= 3; ++y)
        for (int x = -3; x <= 3; ++x)
            CHECK(sameSector(generateSector(42, {x, y}), generateSector(42, {x, y})));
}

TEST_CASE("different seeds and coordinates give different sectors") {
    int differentSeed = 0, differentCoord = 0;
    for (int i = 0; i < 20; ++i) {
        if (!sameSector(generateSector(1, {i, 0}), generateSector(2, {i, 0}))) ++differentSeed;
        if (!sameSector(generateSector(1, {i, 0}), generateSector(1, {i + 1, 0}))) ++differentCoord;
    }
    CHECK(differentSeed >= 19);
    CHECK(differentCoord >= 19);
    CHECK(hashSector(1, {1, 2}) != hashSector(1, {2, 1}));
}

TEST_CASE("sector contents are in bounds and varied") {
    int withStar = 0, withRocks = 0, withNebula = 0, withPlanet = 0;
    for (int y = -10; y < 10; ++y)
        for (int x = -10; x < 10; ++x) {
            const Sector s = generateSector(7, {x, y});
            bool rocks = false, nebula = false, planet = false;
            for (const SectorObject& o : s.objects) {
                CHECK(std::abs(o.pos.x) <= kSectorHalf);
                CHECK(std::abs(o.pos.y) <= kSectorHalf);
                CHECK(o.variant >= 0);
                CHECK(o.variant < kShapeVariants);
                rocks |= o.kind == ObjectKind::Rock;
                nebula |= o.kind == ObjectKind::Nebula;
                planet |= o.kind == ObjectKind::Planet;
            }
            if (s.starIndex >= 0) {
                ++withStar;
                CHECK(s.objects[static_cast<std::size_t>(s.starIndex)].kind == ObjectKind::Star);
            }
            withRocks += rocks;
            withNebula += nebula;
            withPlanet += planet;
        }
    CHECK(withStar > 100);
    CHECK(withStar < 350);
    CHECK(withRocks > 50);
    CHECK(withNebula > 50);
    CHECK(withPlanet > 30);
}

TEST_CASE("the start sector is clear around the ship") {
    for (std::uint64_t seed = 1; seed <= 50; ++seed) {
        const Sector s = generateSector(seed, {0, 0});
        for (const SectorObject& o : s.objects) {
            if (o.kind == ObjectKind::Nebula || o.kind == ObjectKind::Rock || o.kind == ObjectKind::Debris)
                continue; // rocks and debris are checked below
            CHECK(glm::length(o.pos) >= o.radius + 400.0f);
        }
        for (const SectorObject& o : s.objects)
            if (o.kind == ObjectKind::Rock || o.kind == ObjectKind::Debris)
                CHECK(glm::length(o.pos) >= o.radius + 400.0f);
    }
}

TEST_CASE("galaxy home is far away and the hint is vague but consistent") {
    const Galaxy g = makeGalaxy(99);
    const double d = std::hypot(g.home.x, g.home.y);
    CHECK(d >= 29.0);
    CHECK(d <= 46.0);
    CHECK(makeGalaxy(99).home == g.home);

    Galaxy east{1, {40, 0}};
    CHECK(std::string(homeHint(east, {0, 0}).compass) == "EAST");
    CHECK(homeHint(east, {0, 0}).roughSectors == 40);
    Galaxy north{1, {0, -20}};
    CHECK(std::string(homeHint(north, {0, 0}).compass) == "NORTH"); // Y-down: north is -y
    CHECK(std::string(homeHint(Galaxy{1, {10, 10}}, {0, 0}).compass) == "SOUTHEAST");
    CHECK(std::string(homeHint(Galaxy{1, {-10, -10}}, {0, 0}).compass) == "NORTHWEST");
    CHECK(homeHint(east, {40, 0}).roughSectors == 0);
}

TEST_CASE("streamer loads the core immediately, budgets the rest, and evicts") {
    SectorStreamer st(5);
    st.update({0, 0}, {-4, -4}, {4, 4}, 3);
    CHECK(st.find({1, 1}) != nullptr);
    CHECK(st.find({-1, 0}) != nullptr);
    CHECK(st.loadedCount() == 9 + 3);

    for (int i = 0; i < 40; ++i) st.update({0, 0}, {-4, -4}, {4, 4}, 3);
    CHECK(st.loadedCount() == 81);

    // Move far away: the old area is evicted once it is out of the hysteresis margin.
    for (int i = 0; i < 40; ++i) st.update({20, 20}, {18, 18}, {22, 22}, 3);
    CHECK(st.find({0, 0}) == nullptr);
    CHECK(st.find({20, 20}) != nullptr);
    CHECK(st.loadedCount() <= 49);
}

TEST_CASE("streamer returns identical data for a re-loaded sector") {
    SectorStreamer st(5);
    st.update({0, 0}, {0, 0}, {0, 0}, 0);
    const Sector first = *st.find({0, 0});
    st.update({30, 0}, {30, 0}, {30, 0}, 0);
    CHECK(st.find({0, 0}) == nullptr);
    st.update({0, 0}, {0, 0}, {0, 0}, 0);
    CHECK(sameSector(first, *st.find({0, 0})));
}

TEST_CASE("origin shift keeps relative geometry for ship and camera") {
    Ship ship;
    ship.teleport({2990.0f, 10.0f}, 0.0f);
    ship.setDestination({3500.0f, 10.0f});
    ship.update(1.0 / 60.0);
    const glm::vec2 before = ship.destination() - ship.position();
    ship.shiftOrigin({-kSectorSize, 0.0f});
    CHECK(ship.destination().x - ship.position().x == doctest::Approx(before.x));
    CHECK(ship.position().x < 0.0f);
    CHECK(ship.previousPosition().x < 0.0f);

    Camera2D cam;
    cam.snapTo({100.0f, 50.0f}, 1.0f);
    cam.shiftOrigin({-30.0f, 20.0f});
    CHECK(cam.current().center.x == doctest::Approx(70.0f));
    CHECK(cam.current().center.y == doctest::Approx(70.0f));
    CHECK(cam.view(0.5).center.x == doctest::Approx(70.0f));
}
