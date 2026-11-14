#include "camera/Camera2D.h"

#include <algorithm>
#include <cmath>

namespace ee {

namespace {
constexpr float kZoomRate = 14.0f; // 1/seconds; higher = snappier
constexpr float kPanRate = 10.0f;
constexpr float kSettleEpsilon = 1e-4f;
} // namespace

float Camera2D::clampLogZoom(float logZoom) {
    return std::clamp(logZoom, std::log(kMinZoom), std::log(kMaxZoom));
}

CameraView Camera2D::makeView(const State& s) const {
    return {s.center, std::exp(s.logZoom), viewport_};
}

CameraView Camera2D::current() const { return makeView(curr_); }

CameraView Camera2D::view(double alpha) const {
    const float a = static_cast<float>(alpha);
    State s;
    s.center = prev_.center + (curr_.center - prev_.center) * a;
    s.logZoom = prev_.logZoom + (curr_.logZoom - prev_.logZoom) * a;
    return makeView(s);
}

float Camera2D::targetZoom() const { return std::exp(targetLogZoom_); }

void Camera2D::snapTo(glm::vec2 center, float zoom) {
    curr_.center = center;
    curr_.logZoom = clampLogZoom(std::log(zoom));
    prev_ = curr_;
    targetCenter_ = center;
    targetLogZoom_ = curr_.logZoom;
    anchorActive_ = false;
}

void Camera2D::moveTo(glm::vec2 worldPoint) {
    targetCenter_ = worldPoint;
    anchorActive_ = false;
}

void Camera2D::panByScreenDelta(glm::vec2 screenDelta) {
    const glm::vec2 worldDelta = -screenDelta / std::exp(curr_.logZoom);
    curr_.center += worldDelta;
    prev_.center += worldDelta;
    targetCenter_ = curr_.center;
    anchorActive_ = false;
}

void Camera2D::setTargetLogZoom(glm::vec2 screenPoint, float targetLog) {
    targetLog = clampLogZoom(targetLog);
    if (std::abs(targetLog - targetLogZoom_) < 1e-6f) return;
    targetLogZoom_ = targetLog;
    anchorActive_ = true;
    anchorScreen_ = screenPoint;
    anchorWorld_ = current().screenToWorld(screenPoint);
}

void Camera2D::zoomAt(glm::vec2 screenPoint, float factor) {
    setTargetLogZoom(screenPoint, targetLogZoom_ + std::log(factor));
}

void Camera2D::setZoomAt(glm::vec2 screenPoint, float zoom) {
    setTargetLogZoom(screenPoint, std::log(zoom));
}

void Camera2D::update(double dtSeconds) {
    prev_ = curr_;
    const float dt = static_cast<float>(dtSeconds);

    // Zoom eases in log space.
    curr_.logZoom += (targetLogZoom_ - curr_.logZoom) * (1.0f - std::exp(-kZoomRate * dt));
    const bool settled = std::abs(targetLogZoom_ - curr_.logZoom) < kSettleEpsilon;
    if (settled) curr_.logZoom = targetLogZoom_;

    if (anchorActive_) {
        // Keep the anchored world point under the same screen point.
        const float zoom = std::exp(curr_.logZoom);
        curr_.center = anchorWorld_ - (anchorScreen_ - viewport_ * 0.5f) / zoom;
        targetCenter_ = curr_.center;
        if (settled) anchorActive_ = false;
    } else {
        curr_.center += (targetCenter_ - curr_.center) * (1.0f - std::exp(-kPanRate * dt));
        if (glm::length(targetCenter_ - curr_.center) < kSettleEpsilon)
            curr_.center = targetCenter_;
    }
}

float Camera2D::zoomToUnit(float zoom) {
    const float lo = std::log(kMinZoom);
    const float hi = std::log(kMaxZoom);
    return std::clamp((std::log(zoom) - lo) / (hi - lo), 0.0f, 1.0f);
}

float Camera2D::unitToZoom(float unit) {
    const float lo = std::log(kMinZoom);
    const float hi = std::log(kMaxZoom);
    return std::exp(lo + std::clamp(unit, 0.0f, 1.0f) * (hi - lo));
}

} // namespace ee
