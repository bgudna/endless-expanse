#include <doctest/doctest.h>

#include "core/Random.h"
#include "vector/Shapes.h"
#include "vector/StrokeFont.h"
#include "vector/Transform2D.h"

#include <cctype>
#include <string>

using namespace ee;

TEST_CASE("Transform2D: rotation is clockwise on a Y-down screen") {
    Transform2D t{.rotation = 1.57079632679f};
    const glm::vec2 p = t.apply({1.0f, 0.0f});
    CHECK(p.x == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(p.y == doctest::Approx(1.0f));
}

TEST_CASE("Transform2D: matrix matches apply") {
    Transform2D t{.position = {10.0f, -5.0f}, .rotation = 0.7f, .scale = 3.0f};
    const glm::vec2 a = t.apply({2.0f, 1.0f});
    const glm::vec2 b = glm::vec2(t.matrix() * glm::vec3(2.0f, 1.0f, 1.0f));
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
}

TEST_CASE("Random is deterministic and in range") {
    Random a(42), b(42);
    for (int i = 0; i < 100; ++i) {
        const float f = a.nextFloat();
        CHECK(f == b.nextFloat());
        CHECK(f >= 0.0f);
        CHECK(f < 1.0f);
    }
}

TEST_CASE("placeholder shapes are well formed") {
    const VectorShape all[] = {shapes::ship(), shapes::circle(), shapes::asteroid(1),
                               shapes::starCross(), shapes::planet()};
    for (const VectorShape& s : all) {
        CHECK(s.segmentCount() > 0);
        CHECK(s.boundingRadius() <= 1.0001f);
        CHECK(s.boundingRadius() > 0.3f);
    }
    CHECK(shapes::circle(32).segmentCount() == 32);
}

TEST_CASE("asteroids are deterministic per seed and differ across seeds") {
    const VectorShape a = shapes::asteroid(7), b = shapes::asteroid(7), c = shapes::asteroid(8);
    REQUIRE(a.paths[0].points.size() == b.paths[0].points.size());
    for (std::size_t i = 0; i < a.paths[0].points.size(); ++i)
        CHECK(a.paths[0].points[i] == b.paths[0].points[i]);
    CHECK(a.paths[0].points[0] != c.paths[0].points[0]);
}

TEST_CASE("stroke font: every printable glyph stays inside its cell") {
    const float h = 6.0f; // scale 1
    for (int c = 32; c < 127; ++c) {
        const VectorShape s = StrokeFont::build(std::string(1, static_cast<char>(c)), h);
        INFO("char = " << static_cast<char>(c));
        if (c != ' ') CHECK(!s.paths.empty());
        for (const Polyline& p : s.paths)
            for (const glm::vec2& pt : p.points) {
                CHECK(pt.x >= 0.0f);
                CHECK(pt.x <= StrokeFont::kGlyphWidth);
                CHECK(pt.y >= 0.0f);
                CHECK(pt.y <= StrokeFont::kGlyphHeight);
            }
    }
}

TEST_CASE("stroke font: lowercase matches uppercase") {
    CHECK(StrokeFont::build("abc", 12.0f).segmentCount() ==
          StrokeFont::build("ABC", 12.0f).segmentCount());
}

TEST_CASE("stroke font: measure") {
    // Two glyphs at scale 1: 2 * advance minus trailing gap.
    const glm::vec2 m = StrokeFont::measure("AB", 6.0f);
    CHECK(m.x == doctest::Approx(10.0f));
    CHECK(m.y == doctest::Approx(6.0f));
    const glm::vec2 two = StrokeFont::measure("A\nB", 6.0f);
    CHECK(two.y == doctest::Approx(6.0f + StrokeFont::kLineAdvance));
    CHECK(StrokeFont::measure("AB", 12.0f).x == doctest::Approx(20.0f));
}

TEST_CASE("stroke font: text is placed left to right") {
    const VectorShape s = StrokeFont::build("II", 6.0f);
    float maxX = 0.0f;
    for (const Polyline& p : s.paths)
        for (const glm::vec2& pt : p.points) maxX = std::max(maxX, pt.x);
    CHECK(maxX > StrokeFont::kAdvance);
}
