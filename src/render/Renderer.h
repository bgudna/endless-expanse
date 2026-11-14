#pragma once

#include "render/Colors.h"
#include "render/LineBatch.h"
#include "render/PostProcess.h"

#include <glm/glm.hpp>

#include <string_view>

namespace ee {

// Owns the vector line renderer and bloom post-process. Drawing coordinates are in
// logical points (same units as mouse input); the renderer handles HiDPI scaling.
class Renderer {
public:
    bool init();

    // drawW/H are drawable pixels; pixelRatio = drawable / logical size (2 on Retina).
    void beginFrame(int drawW, int drawH, float pixelRatio);
    void endFrame();

    LineBatch& lines() { return lines_; }

    // View for UI / HUD drawing: 1 unit = 1 logical point, origin top-left.
    glm::mat3 screenView() const;
    // View for world drawing: camera centred on screen, `zoom` pixels per world unit.
    glm::mat3 worldView(glm::vec2 cameraCenter = {0.0f, 0.0f}, float zoom = 1.0f) const;
    glm::vec2 logicalSize() const;

    // Screen-space text (uses the screen view; resets it for subsequent draws).
    void drawText(std::string_view text, glm::vec2 topLeft, float height, const glm::vec4& color,
                  float lineWidth = 1.5f);
    void drawTextCentered(std::string_view text, glm::vec2 center, float height,
                          const glm::vec4& color, float lineWidth = 1.5f);

    BloomQuality bloomQuality() const { return quality_; }
    void setBloomQuality(BloomQuality q) { quality_ = q; }
    float bloomIntensity() const { return intensity_; }
    void setBloomIntensity(float v) { intensity_ = v; }

private:
    LineBatch lines_;
    PostProcess post_;
    BloomQuality quality_ = BloomQuality::High;
    BloomQuality frameQuality_ = BloomQuality::High;
    float intensity_ = 1.0f;
    int drawW_ = 0;
    int drawH_ = 0;
    float ratio_ = 1.0f;
};

} // namespace ee
