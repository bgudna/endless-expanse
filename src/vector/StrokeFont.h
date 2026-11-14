#pragma once

#include "vector/VectorShape.h"

#include <glm/glm.hpp>

#include <string_view>

namespace ee::StrokeFont {

// Glyph cell is 4 x 6 units on a Y-down grid; characters advance 6 units.
inline constexpr float kGlyphWidth = 4.0f;
inline constexpr float kGlyphHeight = 6.0f;
inline constexpr float kAdvance = 6.0f;
inline constexpr float kLineAdvance = 9.0f;

// Builds text as a vector shape with its top-left corner at the origin.
// `height` is the glyph height in the caller's units. '\n' starts a new line;
// lowercase is drawn as uppercase.
VectorShape build(std::string_view text, float height);

// Width and height of the text as drawn by build().
glm::vec2 measure(std::string_view text, float height);

} // namespace ee::StrokeFont
