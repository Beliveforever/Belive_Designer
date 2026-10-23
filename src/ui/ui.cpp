#include "ui/ui.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/font.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/logger.hpp"
#include "ui/theme.hpp"

namespace designer
{

ImU32 uiAlpha(ImU32 color, float alpha)
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const unsigned a = (color >> 24) & 0xFFu;
    const unsigned result = (unsigned)(a * alpha);
    return (color & 0x00FFFFFFu) | (result << 24);
}

ImU32 uiMix(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float ar = (float)(a & 0xFF), ag = (float)((a >> 8) & 0xFF), ab = (float)((a >> 16) & 0xFF);
    const float aa = (float)((a >> 24) & 0xFF);
    const float br = (float)(b & 0xFF), bg = (float)((b >> 8) & 0xFF), bb = (float)((b >> 16) & 0xFF);
    const float ba = (float)((b >> 24) & 0xFF);
    return IM_COL32((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t),
        (int)(aa + (ba - aa) * t));
}

const Palette& darkPalette()
{
    static const Palette palette = {
        IM_COL32(30, 30, 30, 255),    // windowBg
        IM_COL32(37, 37, 38, 255),    // panel
        IM_COL32(45, 45, 48, 255),    // panelAlt
        IM_COL32(62, 62, 66, 255),    // panelRaised
        IM_COL32(50, 50, 51, 255),    // header
        IM_COL32(60, 60, 60, 255),    // border
        IM_COL32(51, 51, 51, 255),    // borderSoft
        IM_COL32(212, 212, 212, 255), // text
        IM_COL32(160, 160, 160, 255), // textMuted
        IM_COL32(110, 118, 129, 255), // textFaint
        IM_COL32(0, 122, 204, 255),   // accent (#007ACC)
        IM_COL32(31, 138, 210, 255),  // accentHover
        IM_COL32(9, 71, 113, 255),    // accentSoft
        IM_COL32(78, 201, 176, 255),  // teal
        IM_COL32(137, 209, 133, 255), // green
        IM_COL32(244, 135, 113, 255), // red
        IM_COL32(220, 220, 170, 255), // yellow
        IM_COL32(60, 60, 60, 255),    // inputBg
        IM_COL32(42, 45, 46, 255),    // hover
        IM_COL32(9, 71, 113, 255),    // selected
        IM_COL32(30, 30, 30, 255),    // canvas
        IM_COL32(42, 42, 42, 255),    // canvasGrid
        IM_COL32(66, 66, 66, 255),    // scrollbar
    };
    return palette;
}

const Palette& lightPalette()
{
    static const Palette palette = {
        IM_COL32(238, 241, 246, 255), // windowBg
        IM_COL32(255, 255, 255, 255), // panel
        IM_COL32(245, 247, 251, 255), // panelAlt
        IM_COL32(255, 255, 255, 255), // panelRaised
        IM_COL32(248, 250, 252, 255), // header
        IM_COL32(215, 222, 234, 255), // border
        IM_COL32(230, 236, 245, 255), // borderSoft
        IM_COL32(30, 42, 59, 255),    // text
        IM_COL32(94, 110, 133, 255),  // textMuted
        IM_COL32(108, 124, 148, 255), // textFaint
        IM_COL32(37, 99, 235, 255),   // accent
        IM_COL32(59, 130, 246, 255),  // accentHover
        IM_COL32(219, 234, 254, 255), // accentSoft
        IM_COL32(13, 148, 136, 255),  // teal
        IM_COL32(5, 150, 105, 255),   // green
        IM_COL32(220, 38, 38, 255),   // red
        IM_COL32(217, 119, 6, 255),   // yellow
        IM_COL32(243, 246, 251, 255), // inputBg
        IM_COL32(235, 240, 248, 255), // hover
        IM_COL32(219, 234, 254, 255), // selected
        IM_COL32(232, 236, 243, 255), // canvas
        IM_COL32(222, 228, 238, 255), // canvasGrid
        IM_COL32(190, 201, 217, 255), // scrollbar
    };
    return palette;
}

namespace ui
{

namespace
{
float g_textScale = 1.0f;
}

void setTextScale(float scale)
{
    g_textScale = std::clamp(scale, 0.8f, 1.35f);
}

float getTextScale()
{
    return g_textScale;
}

ImFont* font()
{
    ImFont* value = bklv::getDefaultFont();
    if (value == nullptr && ImGui::GetCurrentContext() != nullptr)
        value = ImGui::GetFont();
    return value;
}

ImFont* iconFont()
{
    return bklv::icon::font();
}

float textWidth(const std::string& text, float size)
{
    ImFont* value = font();
    if (value == nullptr || text.empty())
        return text.size() * size * 0.55f;
    return value->CalcTextSizeA(size * g_textScale, FLT_MAX, 0.0f, text.c_str()).x;
}

float lineHeight(float size)
{
    ImFont* value = font();
    if (value == nullptr)
        return size * 1.4f;
    return value->CalcTextSizeA(size * g_textScale, FLT_MAX, 0.0f, "Ag").y;
}

std::string ellipsize(const std::string& text, float size, float maxWidth)
{
    if (textWidth(text, size) <= maxWidth)
        return text;
    const std::string dots = "...";
    float dotWidth = textWidth(dots, size);
    if (dotWidth > maxWidth)
        return std::string();

    std::string result;
    float width = 0.0f;
    size_t index = 0;
    while (index < text.size())
    {
        const unsigned char lead = (unsigned char)text[index];
        size_t length = 1;
        if ((lead & 0xE0) == 0xC0) length = 2;
        else if ((lead & 0xF0) == 0xE0) length = 3;
        else if ((lead & 0xF8) == 0xF0) length = 4;
        if (index + length > text.size()) length = 1;

        const std::string glyph = text.substr(index, length);
        const float glyphWidth = textWidth(glyph, size);
        if (width + glyphWidth + dotWidth > maxWidth)
            break;
        result += glyph;
        width += glyphWidth;
        index += length;
    }
    return result + dots;
}

void text(ImDrawList* drawList, float x, float y, const std::string& text, ImU32 color, float size, float alpha)
{
    ImFont* value = font();
    if (drawList == nullptr || value == nullptr || text.empty())
        return;
    drawList->AddText(value, size * g_textScale, ImVec2(x, y), uiAlpha(color, alpha), text.c_str());
}

void textClipped(ImDrawList* drawList, float x, float y, float width, const std::string& text, ImU32 color,
    float size, float alpha)
{
    if (width <= 0.0f)
        return;
    ui::text(drawList, x, y, ellipsize(text, size, width), color, size, alpha);
}

void textRight(ImDrawList* drawList, float right, float y, const std::string& text, ImU32 color, float size,
    float alpha)
{
    ui::text(drawList, right - textWidth(text, size), y, text, color, size, alpha);
}

void textVC(ImDrawList* drawList, float x, float y, float height, const std::string& text, ImU32 color,
    float size, float alpha)
{
    const float h = lineHeight(size);
    ui::text(drawList, x, y + (height - h) * 0.5f, text, color, size, alpha);
}

void label(ImDrawList* drawList, const Rect& rect, const std::string& text, ImU32 color, float size, int align,
    float alpha)
{
    const float h = lineHeight(size);
    const float y = rect.y + (rect.h - h) * 0.5f;
    if (align == 1)
        ui::text(drawList, rect.x + (rect.w - textWidth(text, size)) * 0.5f, y, text, color, size, alpha);
    else if (align == 2)
        ui::text(drawList, rect.x + rect.w - textWidth(text, size), y, text, color, size, alpha);
    else
        ui::text(drawList, rect.x, y, text, color, size, alpha);
}

void icon(ImDrawList* drawList, float x, float y, const char* name, ImU32 color, float size, float alpha)
{
    ImFont* value = iconFont();
    if (drawList == nullptr || value == nullptr || name == nullptr || name[0] == '\0')
        return;
    const char* glyph = bklv::icon::glyphByName(name);
    if (glyph == nullptr || glyph[0] == '\0')
        return;
    drawList->AddText(value, size * g_textScale, ImVec2(x, y), uiAlpha(color, alpha), glyph);
}

void iconCentered(ImDrawList* drawList, const Rect& rect, const char* name, ImU32 color, float size)
{
    ImFont* value = iconFont();
    if (value == nullptr || name == nullptr || name[0] == '\0')
        return;
    const char* glyph = bklv::icon::glyphByName(name);
    if (glyph == nullptr || glyph[0] == '\0')
        return;
    const ImVec2 bounds = value->CalcTextSizeA(size * g_textScale, FLT_MAX, 0.0f, glyph);
    drawList->AddText(value, size * g_textScale,
        ImVec2(rect.x + (rect.w - bounds.x) * 0.5f, rect.y + (rect.h - bounds.y) * 0.5f), color, glyph);
}

void iconCode(ImDrawList* drawList, float x, float y, unsigned int codepoint, ImU32 color, float size, float alpha)
{
    ImFont* value = iconFont();
    if (drawList == nullptr || value == nullptr || codepoint == 0)
        return;
    const std::string glyph = bklv::icon::utf8Glyph(codepoint);
    if (glyph.empty())
        return;
    drawList->AddText(value, size * g_textScale, ImVec2(x, y), uiAlpha(color, alpha), glyph.c_str());
}

void iconCodeCentered(ImDrawList* drawList, const Rect& rect, unsigned int codepoint, ImU32 color, float size)
{
    ImFont* value = iconFont();
    if (value == nullptr || codepoint == 0)
        return;
    const std::string glyph = bklv::icon::utf8Glyph(codepoint);
    if (glyph.empty())
        return;
    const ImVec2 bounds = value->CalcTextSizeA(size * g_textScale, FLT_MAX, 0.0f, glyph.c_str());
    drawList->AddText(value, size * g_textScale,
        ImVec2(rect.x + (rect.w - bounds.x) * 0.5f, rect.y + (rect.h - bounds.y) * 0.5f), color,
        glyph.c_str());
}

void fill(ImDrawList* drawList, const Rect& rect, ImU32 color, float radius)
{
    if (color == 0 || rect.w <= 0.0f || rect.h <= 0.0f)
        return;
    drawList->AddRectFilled(ImVec2(rect.x, rect.y), ImVec2(rect.x + rect.w, rect.y + rect.h), color,
        std::min(radius, std::min(rect.w, rect.h) * 0.5f));
}

void stroke(ImDrawList* drawList, const Rect& rect, ImU32 color, float radius, float thickness)
{
    if (color == 0 || rect.w <= 0.0f || rect.h <= 0.0f)
        return;
    drawList->AddRect(ImVec2(rect.x + 0.5f, rect.y + 0.5f), ImVec2(rect.x + rect.w - 0.5f, rect.y + rect.h - 0.5f),
        color, std::min(radius, std::min(rect.w, rect.h) * 0.5f), 0, thickness);
}

void vline(ImDrawList* drawList, float x, float y0, float y1, ImU32 color)
{
    drawList->AddLine(ImVec2(x, y0), ImVec2(x, y1), color, 1.0f);
}

void hline(ImDrawList* drawList, float y, float x0, float x1, ImU32 color)
{
    drawList->AddLine(ImVec2(x0, y), ImVec2(x1, y), color, 1.0f);
}

void triangle(ImDrawList* drawList, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 color)
{
    drawList->AddTriangleFilled(p0, p1, p2, color);
}

void chevron(ImDrawList* drawList, float x, float y, float size, bool down, ImU32 color)
{
    ImVec2 points[3];
    if (down)
    {
        points[0] = ImVec2(x - size, y - size * 0.45f);
        points[1] = ImVec2(x, y + size * 0.45f);
        points[2] = ImVec2(x + size, y - size * 0.45f);
    }
    else
    {
        points[0] = ImVec2(x - size, y + size * 0.45f);
        points[1] = ImVec2(x, y - size * 0.45f);
        points[2] = ImVec2(x + size, y + size * 0.45f);
    }
    drawList->AddPolyline(points, 3, color, 0, 1.6f);
}

void check(ImDrawList* drawList, const Rect& rect, ImU32 color, float thickness)
{
    drawList->AddLine(ImVec2(rect.x + rect.w * 0.18f, rect.y + rect.h * 0.52f),
        ImVec2(rect.x + rect.w * 0.42f, rect.y + rect.h * 0.76f), color, thickness);
    drawList->AddLine(ImVec2(rect.x + rect.w * 0.42f, rect.y + rect.h * 0.76f),
        ImVec2(rect.x + rect.w * 0.84f, rect.y + rect.h * 0.24f), color, thickness);
}

void dot(ImDrawList* drawList, float cx, float cy, float radius, ImU32 color)
{
    drawList->AddCircleFilled(ImVec2(cx, cy), radius, color);
}

void scrollbar(ImDrawList* drawList, const Rect& track, float offset, float contentHeight, float viewHeight,
    ImU32 color)
{
    if (contentHeight <= viewHeight + 1.0f || track.h <= 0.0f)
        return;
    const float ratio = std::clamp(viewHeight / contentHeight, 0.08f, 1.0f);
    const float thumbHeight = std::max(18.0f, track.h * ratio);
    const float maxScroll = std::max(0.001f, contentHeight - viewHeight);
    const float t = std::clamp(offset / maxScroll, 0.0f, 1.0f);
    const float y = track.y + (track.h - thumbHeight) * t;
    Rect thumb(track.x + track.w - 5.0f, y, 4.0f, thumbHeight);
    fill(drawList, thumb, color, 2.0f);
}

void softShadow(ImDrawList* drawList, const Rect& rect, float radius, ImU32 color, float spread, float offsetY)
{
    const int steps = 7;
    for (int i = steps; i >= 1; --i)
    {
        const float t = (float)i / (float)steps;
        const float grow = spread * t;
        Rect layer(rect.x - grow, rect.y - grow + offsetY, rect.w + grow * 2.0f, rect.h + grow * 2.0f);
        fill(drawList, layer, uiAlpha(color, 0.055f * (1.0f - t * 0.55f)), radius + grow);
    }
}

} // namespace ui

} // namespace designer
