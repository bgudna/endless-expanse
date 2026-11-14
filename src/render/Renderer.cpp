#include "render/Renderer.h"

#include "core/Log.h"
#include "vector/StrokeFont.h"

namespace ee {

bool Renderer::init() {
    if (!lines_.init()) return false;
    if (!post_.init()) return false;
    return true;
}

void Renderer::beginFrame(int drawW, int drawH, float pixelRatio) {
    if (drawW != drawW_ || drawH != drawH_) {
        drawW_ = drawW;
        drawH_ = drawH;
        post_.resize(drawW, drawH);
    }
    ratio_ = pixelRatio;
    frameQuality_ = quality_;

    if (frameQuality_ != BloomQuality::Off)
        post_.bindScene();
    else
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, drawW_, drawH_);
    glClearColor(0.01f, 0.012f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Additive blending: overlapping strokes brighten, like a phosphor vector display.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    lines_.begin(drawW_, drawH_, ratio_);
    lines_.setView(screenView());
}

void Renderer::endFrame() {
    lines_.end();
    if (frameQuality_ != BloomQuality::Off) post_.resolve(frameQuality_, intensity_);
}

glm::vec2 Renderer::logicalSize() const {
    return {static_cast<float>(drawW_) / ratio_, static_cast<float>(drawH_) / ratio_};
}

glm::mat3 Renderer::screenView() const {
    glm::mat3 m(1.0f);
    m[0][0] = ratio_;
    m[1][1] = ratio_;
    return m;
}

glm::mat3 Renderer::worldView(glm::vec2 cameraCenter, float zoom) const {
    const float s = ratio_ * zoom;
    const glm::vec2 half = logicalSize() * 0.5f;
    glm::mat3 m(1.0f);
    m[0][0] = s;
    m[1][1] = s;
    m[2] = glm::vec3(ratio_ * half.x - cameraCenter.x * s, ratio_ * half.y - cameraCenter.y * s,
                     1.0f);
    return m;
}

void Renderer::drawText(std::string_view text, glm::vec2 topLeft, float height,
                        const glm::vec4& color, float lineWidth) {
    lines_.setView(screenView());
    lines_.shape(StrokeFont::build(text, height), Transform2D{.position = topLeft}, color,
                 lineWidth);
}

void Renderer::drawTextCentered(std::string_view text, glm::vec2 center, float height,
                                const glm::vec4& color, float lineWidth) {
    drawText(text, center - StrokeFont::measure(text, height) * 0.5f, height, color, lineWidth);
}

} // namespace ee
