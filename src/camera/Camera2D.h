#pragma once

#include <glm/glm.hpp>

namespace ee {

// A snapshot of the camera used for drawing and picking. All coordinates are in
// logical points; world space is Y-down (see docs/decisions.md).
struct CameraView {
    glm::vec2 center{0.0f}; // world point at the middle of the viewport
    float zoom = 1.0f;      // screen points per world unit
    glm::vec2 viewport{1280.0f, 720.0f};

    glm::vec2 worldToScreen(glm::vec2 w) const { return (w - center) * zoom + viewport * 0.5f; }
    glm::vec2 screenToWorld(glm::vec2 s) const { return (s - viewport * 0.5f) / zoom + center; }
    float screenRadius(float worldRadius) const { return worldRadius * zoom; }

    // True if a circle (world position + radius) overlaps the viewport.
    bool isVisible(glm::vec2 worldPos, float worldRadius) const {
        const glm::vec2 s = worldToScreen(worldPos);
        const float r = worldRadius * zoom;
        return s.x + r >= 0.0f && s.x - r <= viewport.x && s.y + r >= 0.0f &&
               s.y - r <= viewport.y;
    }
};

// Smooth pan/zoom camera. Update it with the fixed simulation step; render with
// view(alpha) to interpolate between steps. Zoom is smoothed in log space so that
// zooming feels uniform at every scale, and zooming keeps the world point under
// the cursor fixed.
class Camera2D {
public:
    static constexpr float kMinZoom = 0.02f; // far out: whole sector
    static constexpr float kMaxZoom = 4.0f;  // close tactical view

    void setViewport(glm::vec2 logicalSize) { viewport_ = logicalSize; }

    // Jump instantly (no smoothing).
    void snapTo(glm::vec2 center, float zoom);
    // Smoothly move the view centre to a world point; cancels any zoom anchor.
    void moveTo(glm::vec2 worldPoint);
    // Set where the view should settle (e.g. the player ship) without cancelling an
    // in-progress zoom anchor. Call every step while following.
    void setFollowTarget(glm::vec2 worldPoint) { targetCenter_ = worldPoint; }
    // Drag: the world follows the cursor by `screenDelta` points, immediately.
    void panByScreenDelta(glm::vec2 screenDelta);
    // Multiply the target zoom by `factor`, keeping the world point under `screenPoint` fixed.
    void zoomAt(glm::vec2 screenPoint, float factor);
    // Set an absolute target zoom, keeping the world point under `screenPoint` fixed.
    void setZoomAt(glm::vec2 screenPoint, float zoom);

    void update(double dt);

    CameraView view(double alpha) const; // interpolated, for rendering
    CameraView current() const;          // latest simulated state, for input/picking
    float targetZoom() const;

    // Maps zoom to [0,1] on a log scale (1 = fully zoomed in) and back, for UI sliders.
    static float zoomToUnit(float zoom);
    static float unitToZoom(float unit);

private:
    struct State {
        glm::vec2 center{0.0f};
        float logZoom = 0.0f;
    };

    static float clampLogZoom(float logZoom);
    CameraView makeView(const State& s) const;
    void setTargetLogZoom(glm::vec2 screenPoint, float targetLog);

    State prev_, curr_;
    glm::vec2 targetCenter_{0.0f};
    float targetLogZoom_ = 0.0f;
    bool anchorActive_ = false;
    glm::vec2 anchorWorld_{0.0f};
    glm::vec2 anchorScreen_{0.0f};
    glm::vec2 viewport_{1280.0f, 720.0f};
};

} // namespace ee
