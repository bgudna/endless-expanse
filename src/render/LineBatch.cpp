#include "render/LineBatch.h"

#include "core/Log.h"

#include <glm/gtc/type_ptr.hpp>

namespace ee {

namespace {

constexpr std::size_t kMaxInstancesPerFlush = 65536;

const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec4 i_seg;    // p0.xy, p1.xy in draw space
layout(location = 1) in vec4 i_color;
layout(location = 2) in float i_width; // pixels

uniform mat3 u_view;
uniform vec2 u_viewport;

out vec4 v_color;
out vec2 v_local;          // (along, across) in pixels from p0
flat out float v_len;
flat out float v_halfWidth;

void main() {
    vec2 a = (u_view * vec3(i_seg.xy, 1.0)).xy;
    vec2 b = (u_view * vec3(i_seg.zw, 1.0)).xy;
    vec2 d = b - a;
    float len = length(d);
    vec2 dir = len > 1e-5 ? d / len : vec2(1.0, 0.0);
    vec2 n = vec2(-dir.y, dir.x);

    float hw = max(i_width * 0.5, 0.5);
    float pad = hw + 1.0;

    float sx = float(gl_VertexID & 1);
    float sy = float((gl_VertexID >> 1) & 1);
    float u = mix(-pad, len + pad, sx);
    float v = mix(-pad, pad, sy);

    vec2 p = a + dir * u + n * v;
    vec2 ndc = vec2(p.x / u_viewport.x * 2.0 - 1.0, 1.0 - p.y / u_viewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);

    v_color = i_color;
    v_local = vec2(u, v);
    v_len = len;
    v_halfWidth = hw;
}
)";

const char* kFragmentSrc = R"(#version 330 core
in vec4 v_color;
in vec2 v_local;
flat in float v_len;
flat in float v_halfWidth;
out vec4 o_color;

void main() {
    // Distance to the segment (a capsule): gives round caps and anti-aliased edges.
    float dx = v_local.x - clamp(v_local.x, 0.0, v_len);
    float d = length(vec2(dx, v_local.y));
    float coverage = clamp(v_halfWidth + 0.5 - d, 0.0, 1.0);
    o_color = vec4(v_color.rgb, v_color.a * coverage);
}
)";

} // namespace

bool LineBatch::init() {
    if (!program_.build("lines", kVertexSrc, kFragmentSrc)) return false;
    uView_ = program_.uniform("u_view");
    uViewport_ = program_.uniform("u_viewport");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const GLsizei stride = static_cast<GLsizei>(sizeof(Instance));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(16));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(32));
    for (GLuint i = 0; i < 3; ++i) glVertexAttribDivisor(i, 1);

    glBindVertexArray(0);
    instances_.reserve(kMaxInstancesPerFlush);
    return true;
}

void LineBatch::shutdown() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vbo_ = vao_ = 0;
    program_.destroy();
}

void LineBatch::begin(int viewportW, int viewportH, float widthScale) {
    viewport_ = glm::vec2(static_cast<float>(viewportW), static_cast<float>(viewportH));
    widthScale_ = widthScale;
    view_ = glm::mat3(1.0f);
    last_ = current_;
    current_ = Stats{};
    instances_.clear();
}

void LineBatch::end() { flush(); }

void LineBatch::setView(const glm::mat3& view) {
    if (view == view_) return;
    flush();
    view_ = view;
}

void LineBatch::line(glm::vec2 a, glm::vec2 b, const glm::vec4& c, float width) {
    if (instances_.size() >= kMaxInstancesPerFlush) flush();
    instances_.push_back({a.x, a.y, b.x, b.y, c.r, c.g, c.b, c.a, width * widthScale_});
}

void LineBatch::polyline(const Polyline& path, const glm::mat3& model, const glm::vec4& color,
                         float width) {
    const std::size_t n = path.points.size();
    if (n < 2) return;
    const glm::vec4 c(glm::vec3(color) * path.intensity, color.a);
    auto xf = [&](const glm::vec2& p) { return glm::vec2(model * glm::vec3(p, 1.0f)); };

    glm::vec2 prev = xf(path.points[0]);
    for (std::size_t i = 1; i < n; ++i) {
        const glm::vec2 cur = xf(path.points[i]);
        line(prev, cur, c, width);
        prev = cur;
    }
    if (path.closed) line(prev, xf(path.points[0]), c, width);
}

void LineBatch::shape(const VectorShape& shape, const Transform2D& transform,
                      const glm::vec4& color, float width) {
    const glm::mat3 m = transform.matrix();
    for (const Polyline& p : shape.paths) polyline(p, m, color, width);
}

void LineBatch::flush() {
    if (instances_.empty()) return;

    program_.use();
    glUniformMatrix3fv(uView_, 1, GL_FALSE, glm::value_ptr(view_));
    glUniform2f(uViewport_, viewport_.x, viewport_.y);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances_.size() * sizeof(Instance)),
                 instances_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(instances_.size()));
    glBindVertexArray(0);

    current_.segments += instances_.size();
    ++current_.drawCalls;
    instances_.clear();
}

} // namespace ee
