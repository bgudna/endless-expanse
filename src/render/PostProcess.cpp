#include "render/PostProcess.h"

#include "core/Log.h"

#include <algorithm>

namespace ee {

const char* toString(BloomQuality q) {
    switch (q) {
    case BloomQuality::Off: return "OFF";
    case BloomQuality::Low: return "LOW";
    case BloomQuality::High: return "HIGH";
    }
    return "?";
}

BloomQuality nextQuality(BloomQuality q) {
    switch (q) {
    case BloomQuality::Off: return BloomQuality::Low;
    case BloomQuality::Low: return BloomQuality::High;
    case BloomQuality::High: return BloomQuality::Off;
    }
    return BloomQuality::Off;
}

namespace {

const char* kFullscreenVs = R"(#version 330 core
out vec2 v_uv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// 4 bilinear taps one source texel away from the target pixel centre: a soft 2x downsample.
const char* kDownFs = R"(#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_tex;
uniform vec2 u_texel;
void main() {
    vec4 c = texture(u_tex, v_uv + u_texel * vec2(-1.0, -1.0))
           + texture(u_tex, v_uv + u_texel * vec2( 1.0, -1.0))
           + texture(u_tex, v_uv + u_texel * vec2(-1.0,  1.0))
           + texture(u_tex, v_uv + u_texel * vec2( 1.0,  1.0));
    o_color = c * 0.25;
}
)";

// 9-tap gaussian using 5 bilinear taps.
const char* kBlurFs = R"(#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_tex;
uniform vec2 u_dir; // texel size * direction
void main() {
    vec4 c = texture(u_tex, v_uv) * 0.2270270270;
    c += texture(u_tex, v_uv + u_dir * 1.3846153846) * 0.3162162162;
    c += texture(u_tex, v_uv - u_dir * 1.3846153846) * 0.3162162162;
    c += texture(u_tex, v_uv + u_dir * 3.2307692308) * 0.0702702703;
    c += texture(u_tex, v_uv - u_dir * 3.2307692308) * 0.0702702703;
    o_color = c;
}
)";

const char* kCompositeFs = R"(#version 330 core
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_scene;
uniform sampler2D u_bloom0;
uniform sampler2D u_bloom1;
uniform sampler2D u_bloom2;
uniform vec3 u_weights;
uniform float u_intensity;
void main() {
    vec3 scene = texture(u_scene, v_uv).rgb;
    vec3 bloom = texture(u_bloom0, v_uv).rgb * u_weights.x
               + texture(u_bloom1, v_uv).rgb * u_weights.y
               + texture(u_bloom2, v_uv).rgb * u_weights.z;
    o_color = vec4(scene + bloom * u_intensity, 1.0);
}
)";

void bindTexture(int unit, GLuint tex) {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    glBindTexture(GL_TEXTURE_2D, tex);
}

} // namespace

bool PostProcess::init() {
    if (!down_.build("bloom-down", kFullscreenVs, kDownFs) ||
        !blur_.build("bloom-blur", kFullscreenVs, kBlurFs) ||
        !composite_.build("bloom-composite", kFullscreenVs, kCompositeFs))
        return false;

    uDownTex_ = down_.uniform("u_tex");
    uDownTexel_ = down_.uniform("u_texel");
    uBlurTex_ = blur_.uniform("u_tex");
    uBlurDir_ = blur_.uniform("u_dir");
    uCompScene_ = composite_.uniform("u_scene");
    uCompBloom_[0] = composite_.uniform("u_bloom0");
    uCompBloom_[1] = composite_.uniform("u_bloom1");
    uCompBloom_[2] = composite_.uniform("u_bloom2");
    uCompWeights_ = composite_.uniform("u_weights");
    uCompIntensity_ = composite_.uniform("u_intensity");

    glGenVertexArrays(1, &emptyVao_);
    return true;
}

void PostProcess::shutdown() {
    destroyTarget(scene_);
    for (int i = 0; i < kLevels; ++i) {
        destroyTarget(levelA_[i]);
        destroyTarget(levelB_[i]);
    }
    if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
    emptyVao_ = 0;
    down_.destroy();
    blur_.destroy();
    composite_.destroy();
}

bool PostProcess::createTarget(Target& t, int w, int h) {
    destroyTarget(t);
    t.w = std::max(w, 1);
    t.h = std::max(h, 1);
    glGenTextures(1, &t.tex);
    glBindTexture(GL_TEXTURE_2D, t.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, t.w, t.h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!ok) EE_LOG_ERROR("Framebuffer incomplete (%dx%d)", t.w, t.h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void PostProcess::destroyTarget(Target& t) {
    if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}

void PostProcess::resize(int width, int height) {
    createTarget(scene_, width, height);
    for (int i = 0; i < kLevels; ++i) {
        const int shift = i + 1;
        createTarget(levelA_[i], width >> shift, height >> shift);
        createTarget(levelB_[i], width >> shift, height >> shift);
    }
}

void PostProcess::bindScene() const {
    glBindFramebuffer(GL_FRAMEBUFFER, scene_.fbo);
}

void PostProcess::drawFullscreen() const {
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PostProcess::resolve(BloomQuality quality, float intensity) {
    glDisable(GL_BLEND);
    const int levels = (quality == BloomQuality::High) ? kLevels : 1;

    const Target* src = &scene_;
    for (int i = 0; i < levels; ++i) {
        Target& a = levelA_[i];
        Target& b = levelB_[i];

        // Downsample src -> a.
        glBindFramebuffer(GL_FRAMEBUFFER, a.fbo);
        glViewport(0, 0, a.w, a.h);
        down_.use();
        bindTexture(0, src->tex);
        glUniform1i(uDownTex_, 0);
        glUniform2f(uDownTexel_, 1.0f / static_cast<float>(src->w),
                    1.0f / static_cast<float>(src->h));
        drawFullscreen();

        // Blur a -> b (horizontal), b -> a (vertical).
        blur_.use();
        glUniform1i(uBlurTex_, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, b.fbo);
        bindTexture(0, a.tex);
        glUniform2f(uBlurDir_, 1.0f / static_cast<float>(a.w), 0.0f);
        drawFullscreen();

        glBindFramebuffer(GL_FRAMEBUFFER, a.fbo);
        bindTexture(0, b.tex);
        glUniform2f(uBlurDir_, 0.0f, 1.0f / static_cast<float>(a.h));
        drawFullscreen();

        src = &a;
    }

    // Composite scene + bloom to the default framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scene_.w, scene_.h);
    composite_.use();
    bindTexture(0, scene_.tex);
    glUniform1i(uCompScene_, 0);
    for (int i = 0; i < kLevels; ++i) {
        // Unused levels stay bound to level 0 with a zero weight.
        bindTexture(1 + i, levelA_[i < levels ? i : 0].tex);
        glUniform1i(uCompBloom_[i], 1 + i);
    }
    if (quality == BloomQuality::High)
        glUniform3f(uCompWeights_, 0.55f, 0.45f, 0.35f);
    else
        glUniform3f(uCompWeights_, 0.9f, 0.0f, 0.0f);
    glUniform1f(uCompIntensity_, intensity);
    drawFullscreen();
    glActiveTexture(GL_TEXTURE0);
}

} // namespace ee
