#include <doctest/doctest.h>

#include "core/MathUtil.h"
#include "sim/ExhaustTrail.h"
#include "sim/Ship.h"

#include <algorithm>
#include <cmath>

using namespace ee;

namespace {
constexpr double kDt = 1.0 / 60.0;

// Run until the autopilot finishes or `maxSeconds` pass. Returns elapsed seconds.
double flyToDestination(Ship& s, double maxSeconds) {
    int steps = 0;
    const int maxSteps = static_cast<int>(maxSeconds / kDt);
    while (s.hasDestination() && steps < maxSteps) {
        s.update(kDt);
        ++steps;
    }
    return steps * kDt;
}
} // namespace

TEST_CASE("autopilot arrives at the destination and stops") {
    struct Case {
        float distance;
        float heading;
    };
    const Case cases[] = {{50, 0},      {50, kPi},        {300, 0.0f},      {300, kPi},
                          {300, kPi / 2}, {2000, kPi},    {2000, -kPi / 2}, {20000, 1.0f},
                          {5, 0},       {1, kPi}};
    for (const Case& c : cases) {
        INFO("distance " << c.distance << ", heading " << c.heading);
        Ship s;
        s.teleport({0.0f, 0.0f}, c.heading);
        s.setDestination({c.distance, 0.0f});
        const double t = flyToDestination(s, 120.0);
        CHECK_FALSE(s.hasDestination());
        CHECK(t < 120.0);
        CHECK(s.speed() == doctest::Approx(0.0f).epsilon(1e-3));
        CHECK(glm::length(s.position() - glm::vec2(c.distance, 0.0f)) <= 4.0f + 1e-3f);
    }
}

TEST_CASE("autopilot does not overshoot or oscillate around the target") {
    for (float heading : {0.0f, kPi / 2, kPi}) {
        for (float distance : {200.0f, 1500.0f, 8000.0f}) {
            INFO("distance " << distance << ", heading " << heading);
            Ship s;
            s.teleport({0.0f, 0.0f}, heading);
            const glm::vec2 target(distance, 0.0f);
            s.setDestination(target);

            float closest = distance;
            float worstOvershoot = 0.0f;
            int dirChanges = 0; // times the ship moved away from the target after getting close
            float prevDist = distance;
            bool wasApproaching = true;
            while (s.hasDestination()) {
                s.update(kDt);
                const float d = glm::length(s.position() - target);
                closest = std::min(closest, d);
                if (closest < 30.0f) {
                    worstOvershoot = std::max(worstOvershoot, d - closest);
                    const bool approaching = d <= prevDist + 1e-4f;
                    if (wasApproaching && !approaching) ++dirChanges;
                    wasApproaching = approaching;
                }
                prevDist = d;
                if (s.speed() > 1000.0f) break; // safety
                if (closest == distance && prevDist > distance * 3.0f) break;
            }
            CHECK(worstOvershoot < 10.0f);
            CHECK(dirChanges <= 1);
        }
    }
}

TEST_CASE("autopilot is deterministic") {
    auto run = [] {
        Ship s;
        s.teleport({10.0f, -5.0f}, 2.0f);
        s.setDestination({-800.0f, 400.0f});
        flyToDestination(s, 60.0);
        return s.position();
    };
    const glm::vec2 a = run(), b = run();
    CHECK(a.x == b.x);
    CHECK(a.y == b.y);
}

TEST_CASE("ship never exceeds max speed and turns no faster than turnRate") {
    Ship s;
    s.teleport({0.0f, 0.0f}, kPi);
    s.setDestination({50000.0f, 30000.0f});
    float maxSpeed = 0.0f;
    for (int i = 0; i < 60 * 40; ++i) {
        const float before = s.heading();
        s.update(kDt);
        maxSpeed = std::max(maxSpeed, s.speed());
        const float turned = std::abs(wrapAngle(s.heading() - before));
        CHECK(turned <= s.params().turnRate * static_cast<float>(kDt) + 1e-4f);
    }
    CHECK(maxSpeed <= s.params().maxSpeed + 1e-3f);
    CHECK(maxSpeed > s.params().maxSpeed * 0.9f); // it did reach cruise speed
}

TEST_CASE("ship does not thrust until pointing roughly the right way") {
    Ship s;
    s.teleport({0.0f, 0.0f}, kPi); // facing away
    s.setDestination({1000.0f, 0.0f});
    s.update(kDt);
    CHECK_FALSE(s.thrusting());
    CHECK(s.speed() == doctest::Approx(0.0f));
    // After a half-turn's worth of time it is thrusting toward the target.
    for (int i = 0; i < 120; ++i) s.update(kDt);
    CHECK(s.velocity().x > 0.0f);
}

TEST_CASE("drag assist bleeds off speed when idle; without it the ship drifts") {
    Ship assisted;
    assisted.setVelocity({200.0f, 0.0f});
    for (int i = 0; i < 60 * 15; ++i) assisted.update(kDt);
    CHECK(assisted.speed() == doctest::Approx(0.0f));

    Ship drifting;
    drifting.setDragAssist(false);
    drifting.setVelocity({200.0f, 0.0f});
    for (int i = 0; i < 60 * 15; ++i) drifting.update(kDt);
    CHECK(drifting.speed() == doctest::Approx(200.0f));
    CHECK(drifting.position().x == doctest::Approx(3000.0f).epsilon(1e-3));
}

TEST_CASE("orbit: circles the destination at its radius without turning back") {
    for (float heading : {0.0f, kPi / 2, kPi, -kPi / 2}) {
        for (float distance : {200.0f, 1500.0f}) {
            INFO("distance " << distance << ", heading " << heading);
            Ship s;
            s.teleport({0.0f, 0.0f}, heading);
            const glm::vec2 centre(distance, 0.0f);
            const float radius = 120.0f;
            s.setOrbitDestination(centre, radius);

            for (int i = 0; i < 60 * 20; ++i) s.update(kDt); // settle onto the circle
            REQUIRE(s.hasDestination());
            REQUIRE(s.orbiting());

            int spinChanges = 0;
            float prevSpin = 0.0f;
            float minDist = 1e9f, maxDist = 0.0f;
            for (int i = 0; i < 60 * 30; ++i) {
                s.update(kDt);
                const glm::vec2 rel = s.position() - centre;
                const float d = glm::length(rel);
                minDist = std::min(minDist, d);
                maxDist = std::max(maxDist, d);
                const float spin = rel.x * s.velocity().y - rel.y * s.velocity().x;
                if (i > 0 && spin * prevSpin < 0.0f) ++spinChanges;
                prevSpin = spin;
            }
            CHECK(spinChanges == 0);
            CHECK(minDist > radius * 0.8f);
            CHECK(maxDist < radius * 1.2f);
            CHECK(s.speed() == doctest::Approx(s.params().orbitSpeed).epsilon(0.15));
        }
    }
}

TEST_CASE("orbit: clearing the destination ends the orbit") {
    Ship s;
    s.teleport({0.0f, 0.0f}, 0.0f);
    s.setOrbitDestination({500.0f, 0.0f}, 120.0f);
    for (int i = 0; i < 60 * 10; ++i) s.update(kDt);
    s.clearDestination();
    CHECK_FALSE(s.orbiting());
    for (int i = 0; i < 60 * 15; ++i) s.update(kDt);
    CHECK(s.speed() == doctest::Approx(0.0f));
}

TEST_CASE("orbit: follows a moving centre and keeps its radius band") {
    Ship s;
    s.teleport({0.0f, 0.0f}, 0.0f);
    const glm::vec2 centreVel(200.0f, 0.0f);
    glm::vec2 centre(800.0f, 300.0f);
    const float radius = 120.0f;
    auto step = [&] {
        centre += centreVel * static_cast<float>(kDt);
        s.setOrbitDestination(centre, radius, centreVel);
        s.update(kDt);
    };
    for (int i = 0; i < 60 * 25; ++i) step(); // catch up and settle
    REQUIRE(s.orbiting());

    float minDist = 1e9f, maxDist = 0.0f, minSpeed = 1e9f, maxSpeed = 0.0f;
    for (int i = 0; i < 60 * 30; ++i) {
        step();
        const float d = glm::length(s.position() - centre);
        minDist = std::min(minDist, d);
        maxDist = std::max(maxDist, d);
        const float rel = glm::length(s.velocity() - centreVel);
        minSpeed = std::min(minSpeed, rel);
        maxSpeed = std::max(maxSpeed, rel);
    }
    // Wider band than the static orbit: the ship chases a 200 u/s centre and its turn
    // limit makes the radius wobble by up to ~20 u.
    CHECK(minDist >= radius * 0.75f);
    CHECK(maxDist <= radius * 1.25f);
    CHECK(minSpeed >= 120.0f * 0.85f);
}

TEST_CASE("autopilot can retarget in mid-flight and still arrives") {
    Ship s;
    s.teleport({0.0f, 0.0f}, 0.0f);
    s.setDestination({1500.0f, 0.0f});
    for (int i = 0; i < 60 * 4; ++i) s.update(kDt);
    CHECK(s.speed() > 100.0f);
    const glm::vec2 target(-400.0f, 600.0f);
    s.setDestination(target);
    flyToDestination(s, 120.0);
    CHECK_FALSE(s.hasDestination());
    CHECK(glm::length(s.position() - target) <= 4.0f + 1e-3f);
}

TEST_CASE("clearing the destination lets drag assist take over") {
    Ship s;
    s.teleport({0.0f, 0.0f}, 0.0f);
    s.setDestination({5000.0f, 0.0f});
    for (int i = 0; i < 60 * 5; ++i) s.update(kDt);
    s.clearDestination();
    for (int i = 0; i < 60 * 15; ++i) s.update(kDt);
    CHECK(s.speed() == doctest::Approx(0.0f));
}

TEST_CASE("exhaust tail is capped, spaced and fades out") {
    ExhaustTrail trail;
    for (int i = 0; i < 1000; ++i) trail.emit({static_cast<float>(i) * 10.0f, 0.0f});
    CHECK(trail.points().size() == ExhaustTrail::kMaxPoints);
    trail.emit(trail.points().front().pos); // too close to the newest point: ignored
    CHECK(trail.points().size() == ExhaustTrail::kMaxPoints);
    trail.update(ExhaustTrail::kLife * 0.5f);
    CHECK(trail.points().size() == ExhaustTrail::kMaxPoints);
    trail.update(ExhaustTrail::kLife);
    CHECK(trail.points().empty());
}

TEST_CASE("exhaust tail is ordered newest first") {
    ExhaustTrail trail;
    trail.emit({0.0f, 0.0f});
    trail.update(0.1f);
    trail.emit({100.0f, 0.0f});
    REQUIRE(trail.points().size() == 2);
    CHECK(trail.points()[0].pos.x == doctest::Approx(100.0f));
    CHECK(trail.points()[1].age == doctest::Approx(0.1f));
}
