#include "vector/StrokeFont.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace ee::StrokeFont {

namespace {

// Each glyph is a list of strokes separated by spaces. A stroke is a run of "xy"
// digit pairs (x in 0..4, y in 0..6, y pointing down).
struct GlyphDef {
    char ch;
    const char* strokes;
};

constexpr GlyphDef kGlyphs[] = {
    {' ', ""},
    {'A', "0602204246 0343"},
    {'B', "060030414233033344453606"},
    {'C', "4130100105163645"},
    {'D', "06003041453606"},
    {'E', "40000646 0333"},
    {'F', "400006 0333"},
    {'G', "41301001051636454323"},
    {'H', "0006 4046 0343"},
    {'I', "1030 2026 1636"},
    {'J', "4045361605"},
    {'K', "0006 400346"},
    {'L', "000646"},
    {'M', "0600234046"},
    {'N', "06004640"},
    {'O', "103041453616050110"},
    {'P', "06003041423303"},
    {'Q', "103041453616050110 2446"},
    {'R', "06003041423303 1346"},
    {'S', "413010010213334445361605"},
    {'T', "0040 2026"},
    {'U', "000516364540"},
    {'V', "002640"},
    {'W', "0016233640"},
    {'X', "0046 4006"},
    {'Y', "002340 2326"},
    {'Z', "00400646"},
    {'0', "103041453616050110 1531"},
    {'1', "102026 1636"},
    {'2', "01103041420646"},
    {'3', "004022334445361605"},
    {'4', "300343 3036"},
    {'5', "400002334445361605"},
    {'6', "4130100105163645433303"},
    {'7', "004026"},
    {'8', "103041423313020110 133344453616050413"},
    {'9', "0516364541301001021343"},
    {'.', "2526"},
    {':', "2122 2526"},
    {',', "2536"},
    {'-', "1333"},
    {'+', "0343 2135"},
    {'=', "0242 0444"},
    {'_', "0646"},
    {'/', "0640"},
    {'%', "0640 0001 4445"},
    {'(', "30111536"},
    {')', "10313516"},
    {'<', "410345"},
    {'>', "014305"},
    {'!', "2024 2526"},
    {'?', "01103041422324 2526"},
    {'\'', "2021"},
};

constexpr const char* kUnknownGlyph = "0040460600";

const char* strokesFor(char c) {
    static const std::array<const char*, 128> table = [] {
        std::array<const char*, 128> t{};
        for (const GlyphDef& g : kGlyphs) t[static_cast<std::size_t>(g.ch)] = g.strokes;
        return t;
    }();
    const unsigned char u = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c)));
    if (u >= 128) return kUnknownGlyph;
    const char* s = table[u];
    return s ? s : kUnknownGlyph;
}

void appendGlyph(VectorShape& shape, const char* s, glm::vec2 origin, float scale) {
    Polyline cur;
    auto flush = [&] {
        if (cur.points.size() >= 2) shape.add(std::move(cur));
        cur = Polyline{};
    };
    while (*s) {
        if (*s == ' ') {
            flush();
            ++s;
            continue;
        }
        const float x = static_cast<float>(s[0] - '0');
        const float y = static_cast<float>(s[1] - '0');
        s += 2;
        cur.points.push_back(origin + glm::vec2(x, y) * scale);
    }
    flush();
}

} // namespace

VectorShape build(std::string_view text, float height) {
    const float scale = height / kGlyphHeight;
    VectorShape shape;
    float col = 0.0f;
    float row = 0.0f;
    for (char c : text) {
        if (c == '\n') {
            col = 0.0f;
            row += 1.0f;
            continue;
        }
        const glm::vec2 origin(col * kAdvance * scale, row * kLineAdvance * scale);
        appendGlyph(shape, strokesFor(c), origin, scale);
        col += 1.0f;
    }
    return shape;
}

glm::vec2 measure(std::string_view text, float height) {
    const float scale = height / kGlyphHeight;
    std::size_t maxCols = 0;
    std::size_t cols = 0;
    std::size_t lines = 1;
    for (char c : text) {
        if (c == '\n') {
            ++lines;
            cols = 0;
        } else {
            maxCols = std::max(maxCols, ++cols);
        }
    }
    if (maxCols == 0) return {0.0f, kGlyphHeight * scale};
    const float w = (static_cast<float>(maxCols) * kAdvance - (kAdvance - kGlyphWidth)) * scale;
    const float h = (kGlyphHeight + static_cast<float>(lines - 1) * kLineAdvance) * scale;
    return {w, h};
}

} // namespace ee::StrokeFont
