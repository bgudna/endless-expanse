#include <doctest/doctest.h>

#include "sim/Alien.h"
#include "sim/AlienType.h"
#include "sim/Director.h"
#include "sim/Simulation.h"
#include "sim/Spatial.h"
#include "sim/Vitals.h"

#include <cmath>
#include <fstream>
#include <sstream>

using namespace ee;

namespace {
std::vector<AlienType> loadRealTypes() {
    std::ifstream f(EE_ASSET_DIR "/data/aliens.json");
    std::stringstream ss;
    ss << f.rdbuf();
    std::string err;
    auto t = parseAlienTypes(ss.str(), &err);
    INFO(err);
    return t;
}

AlienType raiderType() {
    AlienType t;
    t.id = "r";
    t.name = "TEST RAIDER";
    t.role = Role::Raider;
    t.hull = 40.0f;
    t.shield = 0.0f;
    t.damage = 5.0f;
    t.range = 500.0f;
    t.sight = 1500.0f;
    return t;
}

bool finite(glm::vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
} // namespace

TEST_CASE("vitals: shield absorbs first, hull is reduced after, shield recharges after a delay") {
    Vitals v = Vitals::make(100.0f, 50.0f);
    v.damage(30.0f);
    CHECK(v.shield == doctest::Approx(20.0f));
    CHECK(v.hull == doctest::Approx(100.0f));
    v.damage(50.0f);
    CHECK(v.shield == doctest::Approx(0.0f));
    CHECK(v.hull == doctest::Approx(70.0f));
    v.damage(10.0f, true); // piercing ignores shield
    CHECK(v.hull == doctest::Approx(60.0f));

    v.update(1.0f);
    CHECK(v.shield == doctest::Approx(0.0f)); // still inside the regen delay
    for (int i = 0; i < 600; ++i) v.update(0.1f);
    CHECK(v.shield == doctest::Approx(50.0f));
    v.damage(1000.0f);
    CHECK(v.hull == doctest::Approx(0.0f));
    CHECK_FALSE(v.alive());
    v.damage(-5.0f);
    CHECK(v.hull == doctest::Approx(0.0f));
}

TEST_CASE("sector grid finds nearby solids and lists zones") {
    Sector s;
    s.objects.push_back({ObjectKind::Rock, {100.0f, 100.0f}, 20.0f, 0.0f, 0, Tint::Amber});
    s.objects.push_back({ObjectKind::Rock, {-2000.0f, 500.0f}, 20.0f, 0.0f, 0, Tint::Amber});
    s.objects.push_back({ObjectKind::Nebula, {0.0f, 0.0f}, 900.0f, 0.0f, 0, Tint::Cyan});
    s.objects.push_back({ObjectKind::Star, {-1000.0f, -1000.0f}, 300.0f, 0.0f, 0, Tint::Amber});
    SectorGrid grid(s);

    std::vector<std::uint16_t> out;
    grid.query({110.0f, 110.0f}, 5.0f, out);
    CHECK(out == std::vector<std::uint16_t>{0});
    out.clear();
    grid.query({-1000.0f, -1000.0f}, 10.0f, out);
    CHECK(out == std::vector<std::uint16_t>{3});
    out.clear();
    grid.query({2500.0f, 2500.0f}, 10.0f, out);
    CHECK(out.empty());
    out.clear();
    grid.query({0.0f, 0.0f}, 3000.0f, out); // whole sector: each object once
    CHECK(out.size() == 3);
    CHECK(grid.zones() == std::vector<std::uint16_t>{2});
}

TEST_CASE("alien types parse from json and reject bad data") {
    std::string err;
    const auto ok = parseAlienTypes(
        R"({"aliens":[{"id":"a","role":"raider","hull":10,"damage":2}]})", &err);
    REQUIRE(ok.size() == 1);
    CHECK(ok[0].id == "a");
    CHECK(ok[0].hull == doctest::Approx(10.0f));
    CHECK(ok[0].name == "a"); // defaults to the id

    CHECK(parseAlienTypes("not json", &err).empty());
    CHECK(!err.empty());
    CHECK(parseAlienTypes(R"({"aliens":[{"id":"a","role":"wizard"}]})", &err).empty());
    CHECK(parseAlienTypes(R"({"aliens":[{"id":"a","role":"raider","hull":0}]})", &err).empty());
    CHECK(parseAlienTypes(R"({"aliens":[]})", &err).empty());

    const auto real = loadRealTypes();
    CHECK(real.size() >= 5);
    bool roles[4] = {};
    for (const AlienType& t : real) roles[static_cast<int>(t.role)] = true;
    CHECK(roles[0]);
    CHECK(roles[1]);
    CHECK(roles[2]);
    CHECK(roles[3]);
}

TEST_CASE("raider AI: patrol, spot, pursue, attack and fire") {
    const AlienType type = raiderType();
    AlienShip a(1, &type, 1);
    a.body.teleport({0.0f, 0.0f}, 0.0f);
    AiContext ctx;
    ctx.playerPos = {3000.0f, 0.0f};

    for (int i = 0; i < 10; ++i) updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Patrol);

    ctx.playerPos = {1200.0f, 0.0f};
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Pursue);

    ctx.playerPos = {400.0f, 0.0f};
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Attack);

    bool fired = false;
    for (int i = 0; i < 120; ++i) {
        const AiOutput out = updateAlien(a, ctx, 1.0 / 60.0);
        if (out.fire) {
            fired = true;
            CHECK(glm::length(out.aimDir) == doctest::Approx(1.0f).epsilon(0.01));
        }
    }
    CHECK(fired);

    // Nebula halves sight: the same raider loses track of a distant player.
    AlienShip b(2, &type, 2);
    b.body.teleport({0.0f, 0.0f}, 0.0f);
    ctx.playerPos = {1000.0f, 0.0f};
    ctx.sightScale = 0.5f;
    updateAlien(b, ctx, 1.0 / 60.0);
    CHECK(b.state == AiState::Patrol);
}

TEST_CASE("raider flees once when badly hurt") {
    const AlienType type = raiderType();
    AlienShip a(1, &type, 1);
    a.body.teleport({0.0f, 0.0f}, 0.0f);
    AiContext ctx;
    ctx.playerPos = {400.0f, 0.0f};
    updateAlien(a, ctx, 1.0 / 60.0);
    a.vitals.damage(35.0f); // 5 of 40 left
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Flee);
    CHECK(a.hasFled);
    a.setState(AiState::Pursue);
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state != AiState::Flee); // does not flee a second time
}

TEST_CASE("trader hails, trades on accept, then leaves") {
    AlienType type;
    type.id = "t";
    type.role = Role::Trader;
    type.damage = 0.0f;
    AlienShip a(1, &type, 1);
    a.body.teleport({0.0f, 0.0f}, 0.0f);
    AiContext ctx;
    ctx.playerPos = {2000.0f, 0.0f};
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Patrol);

    ctx.playerPos = {300.0f, 0.0f};
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Hail);

    a.hailAnswered = true;
    a.hailAccepted = true;
    for (int i = 0; i < 60 * 7; ++i) updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.completedTrade);
    CHECK(a.resolved);
    CHECK(a.leaving);
    CHECK(a.state == AiState::Patrol);
}

TEST_CASE("an unanswered hail times out and the ship leaves") {
    AlienType type;
    type.role = Role::Distress;
    type.damage = 0.0f;
    AlienShip a(1, &type, 1);
    AiContext ctx;
    ctx.playerPos = {200.0f, 0.0f};
    updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.state == AiState::Hail);
    for (int i = 0; i < 60 * 26; ++i) updateAlien(a, ctx, 1.0 / 60.0);
    CHECK(a.resolved);
    CHECK_FALSE(a.completedTrade);
}

TEST_CASE("director: quiet floor, first delay, and long-run rate") {
    DirectorConfig cfg;
    EncounterDirector d(7, cfg);
    const double dt = 1.0;
    double t = 0.0, last = 0.0;
    int count = 0;
    double minGap = 1e9;
    double first = -1.0;
    EncounterKind kind = EncounterKind::PassBy;
    int combat = 0;
    while (t < 36000.0) {
        t += dt;
        if (d.update(dt, false, kind)) {
            if (first < 0.0) first = t;
            else minGap = std::min(minGap, t - last);
            last = t;
            ++count;
            if (isCombat(kind)) ++combat;
            d.update(dt, true, kind); // encounter runs; ends immediately after
        }
    }
    CHECK(first >= cfg.firstDelay);
    CHECK(minGap >= cfg.minQuiet);
    // Mean gap is avgInterval, stretched by the post-combat breather: expect roughly 36000 / 85.
    CHECK(count > 330);
    CHECK(count < 500);
    CHECK(combat > 0);
    CHECK(combat < count);
}

TEST_CASE("director never fires while an encounter is active") {
    EncounterDirector d(1, DirectorConfig{});
    EncounterKind k;
    for (int i = 0; i < 100000; ++i) CHECK_FALSE(d.update(1.0, true, k));
}

TEST_CASE("player collides with a rock: pushed out, bounced and damaged") {
    for (std::uint64_t seed = 1; seed < 40; ++seed) {
        Simulation sim(seed, loadRealTypes());
        const Sector* s = sim.streamer().find({0, 0});
        const SectorObject* rock = nullptr;
        for (const SectorObject& o : s->objects)
            if (o.kind == ObjectKind::Rock) {
                rock = &o;
                break;
            }
        if (!rock) continue;

        sim.player().teleport(rock->pos + glm::vec2(rock->radius + 70.0f, 0.0f), 0.0f);
        sim.player().setVelocity({-300.0f, 0.0f});
        const float total = sim.vitals().hull + sim.vitals().shield;
        for (int i = 0; i < 30; ++i) sim.update(1.0 / 60.0);
        CHECK(sim.vitals().hull + sim.vitals().shield < total);
        CHECK(sim.player().velocity().x > -100.0f); // bounced, no longer flying straight in
        return;
    }
    FAIL("no seed produced a rock in the start sector");
}

TEST_CASE("player triggers a mine, which detonates once and hurts") {
    for (std::uint64_t seed = 1; seed < 800; ++seed) {
        Simulation sim(seed, loadRealTypes());
        sim.setStreamBox({-1, -1}, {1, 1});
        sim.update(1.0 / 60.0);
        for (int sy = -1; sy <= 1; ++sy)
            for (int sx = -1; sx <= 1; ++sx) {
                const Sector* s = sim.streamer().find({sx, sy});
                if (!s) continue;
                for (std::size_t i = 0; i < s->objects.size(); ++i) {
                    if (s->objects[i].kind != ObjectKind::Mine) continue;
                    const glm::vec2 pos = s->objects[i].pos + sectorOffset({sx, sy}, {0, 0});
                    sim.player().teleport(pos + glm::vec2(0.0f, 20.0f), 0.0f);
                    const float before = sim.vitals().hull + sim.vitals().shield;
                    sim.update(1.0 / 60.0);
                    CHECK(sim.isDestroyed({sx, sy}, i));
                    CHECK(sim.vitals().hull + sim.vitals().shield < before);
                    const float after = sim.vitals().hull + sim.vitals().shield;
                    sim.update(1.0 / 60.0);
                    CHECK(sim.vitals().hull + sim.vitals().shield >= after); // no second blast
                    return;
                }
            }
    }
    FAIL("no mine found");
}

TEST_CASE("player shoots and destroys a targeted raider") {
    Simulation sim(3, loadRealTypes());
    // An ambush spawns fast raiders around the player, independent of the director's timing.
    REQUIRE(sim.startEncounter(EncounterKind::Ambush));
    REQUIRE(!sim.aliens().empty());
    sim.setWeaponsFree(true);
    sim.setGodMode(true);
    for (int step = 0; step < 60 * 90 && !sim.aliens().empty(); ++step) {
        if (sim.target() < 0 && !sim.aliens().empty()) sim.setTarget(sim.aliens().front().id);
        sim.update(1.0 / 60.0);
    }
    CHECK(sim.aliens().empty());
    CHECK(sim.kills() >= 2);
}

TEST_CASE("hostile aliens damage the player") {
    Simulation sim(3, loadRealTypes());
    REQUIRE(sim.startEncounter(EncounterKind::Ambush));
    const float total = sim.vitals().hull + sim.vitals().shield;
    for (int step = 0; step < 60 * 20 && sim.vitals().hull + sim.vitals().shield >= total; ++step)
        sim.update(1.0 / 60.0);
    CHECK(sim.vitals().hull + sim.vitals().shield < total);
}

TEST_CASE("headless soak: 10 minutes of simulated play, several seeds") {
    DirectorConfig cfg;
    cfg.avgInterval = 30.0f;
    cfg.minQuiet = 10.0f;
    cfg.firstDelay = 10.0f;

    for (std::uint64_t seed = 11; seed < 14; ++seed) {
        Simulation sim(seed, loadRealTypes(), cfg);
        sim.setGodMode(true);
        sim.setWeaponsFree(true);
        Random script(seed * 77);
        const double dt = 1.0 / 60.0;
        std::size_t maxAliens = 0, maxProjectiles = 0;
        std::size_t eventsSeen = 0;
        int hails = 0;
        int crossings = 0;

        for (int step = 0; step < 60 * 600; ++step) {
            if (step % 600 == 0) { // new far-away waypoint every 10 s
                const float a = script.range(-3.14f, 3.14f);
                sim.player().setDestination(sim.player().position() + glm::vec2(std::cos(a), std::sin(a)) * 20000.0f);
            }
            if (step % 60 == 0) {
                if (sim.target() < 0 && !sim.aliens().empty()) sim.setTarget(sim.aliens().front().id);
                if (sim.hailingAlien()) {
                    ++hails;
                    sim.answerHail(script.nextFloat() < 0.5f);
                }
            }
            // View box like a zoomed-out camera, to exercise streaming and eviction.
            const SectorCoord o = sim.origin();
            sim.setStreamBox({o.x - 3, o.y - 3}, {o.x + 3, o.y + 3});
            sim.update(dt);
            const glm::vec2 shift = sim.takeOriginShift();
            CHECK(finite(shift));
            if (shift != glm::vec2(0.0f)) ++crossings;
            eventsSeen += sim.takeEvents().size();

            REQUIRE(finite(sim.player().position()));
            REQUIRE(std::abs(sim.player().position().x) < kSectorSize * 2.0f);
            REQUIRE(std::abs(sim.player().position().y) < kSectorSize * 2.0f);
            REQUIRE(std::isfinite(sim.vitals().hull));
            REQUIRE(sim.vitals().shield >= 0.0f);
            REQUIRE(sim.vitals().shield <= sim.vitals().maxShield + 1e-3f);
            maxAliens = std::max(maxAliens, sim.aliens().size());
            maxProjectiles = std::max(maxProjectiles, sim.projectiles().size());
            for (const AlienShip& al : sim.aliens()) {
                REQUIRE(finite(al.body.position()));
                REQUIRE(al.vitals.hull >= 0.0f); // killed late in a step: removed next step
            }
            REQUIRE(sim.streamer().loadedCount() <= 11u * 11u);
        }
        CHECK(sim.encountersStarted() >= 5);
        CHECK(sim.encountersStarted() <= 600 / 10 + 1);
        CHECK(maxAliens <= static_cast<std::size_t>(cfg.maxAliens));
        CHECK(maxProjectiles < 500);
        CHECK(eventsSeen > 10);
        CHECK(sim.playerAlive());
        // Travelled far enough to cross many sectors.
        CHECK(crossings >= 5);
        (void)hails;
    }
}
