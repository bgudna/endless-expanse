#include <doctest/doctest.h>

#include "camera/Camera2D.h"

using namespace ee;

namespace {
constexpr double kDt = 1.0 / 60.0;

Camera2D makeCamera() {
    Camera2D c;
    c.setViewport({1000.0f, 600.0f});
    c.snapTo({0.0f, 0.0f}, 1.0f);
    return c;
}

void run(Camera2D& c, int steps) {
    for (int i = 0; i < steps; ++i) c.update(kDt);
}

void checkNear(glm::vec2 a, glm::vec2 b, float eps = 1e-2f) {
    CHECK(a.x == doctest::Approx(b.x).epsilon(eps));
    CHECK(a.y == doctest::Approx(b.y).epsilon(eps));
}
} // namespace

TEST_CASE("CameraView: world/screen round trip") {
    CameraView v{{120.0f, -80.0f}, 0.37f, {1000.0f, 600.0f}};
    const glm::vec2 w(333.0f, 444.0f);
    checkNear(v.screenToWorld(v.worldToScreen(w)), w, 1e-4f);
    // The view centre maps to the middle of the viewport.
    checkNear(v.worldToScreen(v.center), {500.0f, 300.0f}, 1e-4f);
}

TEST_CASE("CameraView: visibility culling") {
    CameraView v{{0.0f, 0.0f}, 1.0f, {1000.0f, 600.0f}};
    CHECK(v.isVisible({0.0f, 0.0f}, 1.0f));
    CHECK(v.isVisible({510.0f, 0.0f}, 20.0f));  // circle straddles the right edge
    CHECK_FALSE(v.isVisible({600.0f, 0.0f}, 20.0f));
    CHECK_FALSE(v.isVisible({0.0f, -400.0f}, 50.0f));
    v.zoom = 0.1f; // zoomed out: far objects become visible
    CHECK(v.isVisible({4000.0f, 0.0f}, 10.0f));
}

TEST_CASE("zoomAt keeps the world point under the cursor fixed, during and after the animation") {
    Camera2D c = makeCamera();
    const glm::vec2 cursor(800.0f, 150.0f);
    const glm::vec2 worldUnderCursor = c.current().screenToWorld(cursor);

    c.zoomAt(cursor, 2.0f);
    for (int i = 0; i < 120; ++i) {
        c.update(kDt);
        checkNear(c.current().worldToScreen(worldUnderCursor), cursor);
    }
    CHECK(c.current().zoom == doctest::Approx(2.0f).epsilon(1e-3));
}

TEST_CASE("zoom is clamped to the allowed range") {
    Camera2D c = makeCamera();
    for (int i = 0; i < 100; ++i) c.zoomAt({500.0f, 300.0f}, 2.0f);
    run(c, 300);
    CHECK(c.current().zoom == doctest::Approx(Camera2D::kMaxZoom).epsilon(1e-3));
    for (int i = 0; i < 200; ++i) c.zoomAt({500.0f, 300.0f}, 0.5f);
    run(c, 300);
    CHECK(c.current().zoom == doctest::Approx(Camera2D::kMinZoom).epsilon(1e-3));
}

TEST_CASE("zoom eases smoothly and monotonically") {
    Camera2D c = makeCamera();
    c.zoomAt({500.0f, 300.0f}, 3.0f);
    float prev = c.current().zoom;
    for (int i = 0; i < 90; ++i) {
        c.update(kDt);
        const float z = c.current().zoom;
        CHECK(z >= prev);
        CHECK(z <= 3.0f + 1e-3f);
        prev = z;
    }
}

TEST_CASE("pan drags the world with the cursor") {
    Camera2D c = makeCamera();
    c.snapTo({0.0f, 0.0f}, 2.0f);
    const glm::vec2 cursor(300.0f, 300.0f);
    const glm::vec2 w = c.current().screenToWorld(cursor);
    c.panByScreenDelta({50.0f, -20.0f});
    checkNear(c.current().worldToScreen(w), cursor + glm::vec2(50.0f, -20.0f), 1e-4f);
    // Panning does not drift afterwards.
    const glm::vec2 after = c.current().center;
    run(c, 60);
    checkNear(c.current().center, after, 1e-4f);
}

TEST_CASE("moveTo converges on the target") {
    Camera2D c = makeCamera();
    c.moveTo({400.0f, -250.0f});
    run(c, 180);
    checkNear(c.current().center, {400.0f, -250.0f}, 1e-3f);
}

TEST_CASE("interpolated view lies between previous and current steps") {
    Camera2D c = makeCamera();
    c.moveTo({100.0f, 0.0f});
    c.update(kDt);
    const float x1 = c.current().center.x;
    c.update(kDt);
    const float x2 = c.current().center.x;
    const float mid = c.view(0.5).center.x;
    CHECK(mid == doctest::Approx((x1 + x2) * 0.5f));
    CHECK(c.view(1.0).center.x == doctest::Approx(x2));
}

TEST_CASE("zoom <-> slider unit mapping round trips and spans the range") {
    CHECK(Camera2D::zoomToUnit(Camera2D::kMinZoom) == doctest::Approx(0.0f));
    CHECK(Camera2D::zoomToUnit(Camera2D::kMaxZoom) == doctest::Approx(1.0f));
    for (float u : {0.0f, 0.2f, 0.5f, 0.9f, 1.0f})
        CHECK(Camera2D::zoomToUnit(Camera2D::unitToZoom(u)) == doctest::Approx(u));
}

TEST_CASE("simulation steps at different rates reach the same zoom") {
    auto settle = [](double dt) {
        Camera2D c = makeCamera();
        c.zoomAt({500.0f, 300.0f}, 2.0f);
        const int steps = static_cast<int>(0.2 / dt + 0.5);
        for (int i = 0; i < steps; ++i) c.update(dt);
        return c.current().zoom;
    };
    CHECK(settle(1.0 / 60.0) == doctest::Approx(settle(1.0 / 120.0)).epsilon(1e-2));
}
