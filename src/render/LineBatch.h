#pragma once

#include "render/Shader.h"
#include "vector/Transform2D.h"
#include "vector/VectorShape.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace ee {

// Batched thick-line renderer. Each segment is one instance; the vertex shader
// expands it into a screen-space quad and the fragment shader shapes it into a
// capsule (anti-aliased, round caps). Line widths are in logical points and stay
// constant regardless of zoom.
class LineBatch {
public:
    struct Stats {
        std::size_t segments = 0;
        std::size_t drawCalls = 0;
    };

    LineBatch() = default;
    ~LineBatch() { shutdown(); }
    LineBatch(const LineBatch&) = delete;
    LineBatch& operator=(const LineBatch&) = delete;

    bool init();
    void shutdown();

    // viewportW/H in drawable pixels; widthScale converts logical points to pixels.
    void begin(int viewportW, int viewportH, float widthScale);
    void end();

    // Matrix mapping draw coordinates to drawable pixels (origin top-left, Y down).
    void setView(const glm::mat3& view);

    void line(glm::vec2 a, glm::vec2 b, const glm::vec4& color, float width);
    void polyline(const Polyline& path, const glm::mat3& model, const glm::vec4& color,
                  float width);
    void shape(const VectorShape& shape, const Transform2D& transform, const glm::vec4& color,
               float width);

    const Stats& lastFrameStats() const { return last_; }

private:
    struct Instance {
        float x0, y0, x1, y1;
        float r, g, b, a;
        float width;
    };
    static_assert(sizeof(Instance) == 36, "Instance layout must match vertex attributes");

    void flush();

    ShaderProgram program_;
    GLint uView_ = -1;
    GLint uViewport_ = -1;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    std::vector<Instance> instances_;
    glm::mat3 view_{1.0f};
    glm::vec2 viewport_{1.0f};
    float widthScale_ = 1.0f;
    Stats current_;
    Stats last_;
};

} // namespace ee
