#pragma once

#include "render/Shader.h"

namespace ee {

enum class BloomQuality { Off, Low, High };

const char* toString(BloomQuality q);
BloomQuality nextQuality(BloomQuality q);

// Offscreen HDR scene target plus a downsample/blur bloom chain.
class PostProcess {
public:
    PostProcess() = default;
    ~PostProcess() { shutdown(); }
    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    bool init();
    void shutdown();
    void resize(int width, int height);

    // Render the scene into the offscreen target.
    void bindScene() const;
    // Blur the scene and composite the result to the default framebuffer.
    void resolve(BloomQuality quality, float intensity);

private:
    static constexpr int kLevels = 3;

    struct Target {
        GLuint fbo = 0;
        GLuint tex = 0;
        int w = 0;
        int h = 0;
    };

    bool createTarget(Target& t, int w, int h);
    void destroyTarget(Target& t);
    void drawFullscreen() const;

    ShaderProgram down_, blur_, composite_;
    GLint uDownTex_ = -1, uDownTexel_ = -1;
    GLint uBlurTex_ = -1, uBlurDir_ = -1;
    GLint uCompScene_ = -1, uCompBloom_[kLevels] = {-1, -1, -1};
    GLint uCompWeights_ = -1, uCompIntensity_ = -1;

    GLuint emptyVao_ = 0;
    Target scene_;
    Target levelA_[kLevels];
    Target levelB_[kLevels];
};

} // namespace ee
