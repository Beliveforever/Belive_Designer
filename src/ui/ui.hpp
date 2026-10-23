#pragma once

#include <string>

#include <imgui.h>

namespace designer
{

struct Rect
{
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;

    bool contains(float px, float py) const
    {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    bool contains(const ImVec2& point) const { return this->contains(point.x, point.y); }
    ImVec2 min() const { return ImVec2(x, y); }
    ImVec2 max() const { return ImVec2(x + w, y + h); }
};

struct UiInput
{
    ImVec2 mouse = ImVec2(0, 0);
    ImVec2 delta = ImVec2(0, 0);
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool rightPressed = false;
    bool doubleClicked = false;
    float wheel = 0;
    bool shift = false;
    bool command = false;
};

ImU32 uiAlpha(ImU32 color, float alpha);
ImU32 uiMix(ImU32 a, ImU32 b, float t);

namespace ui
{

// Global scale applied to all custom-drawn text and icons (does not affect the
// document canvas itself; that is scaled by the canvas zoom).
void setTextScale(float scale);
float getTextScale();

ImFont* font();
ImFont* iconFont();
float textWidth(const std::string& text, float size = 13.0f);
float lineHeight(float size = 13.0f);
std::string ellipsize(const std::string& text, float size, float maxWidth);

void text(ImDrawList* drawList, float x, float y, const std::string& text, ImU32 color, float size = 13.0f,
    float alpha = 1.0f);
void textClipped(ImDrawList* drawList, float x, float y, float width, const std::string& text, ImU32 color,
    float size = 13.0f, float alpha = 1.0f);
void textRight(ImDrawList* drawList, float right, float y, const std::string& text, ImU32 color,
    float size = 13.0f, float alpha = 1.0f);
void textVC(ImDrawList* drawList, float x, float y, float height, const std::string& text, ImU32 color,
    float size = 13.0f, float alpha = 1.0f);
void label(ImDrawList* drawList, const Rect& rect, const std::string& text, ImU32 color, float size = 13.0f,
    int align = 0, float alpha = 1.0f); // align 0 left, 1 center, 2 right

void icon(ImDrawList* drawList, float x, float y, const char* name, ImU32 color, float size = 16.0f,
    float alpha = 1.0f);
void iconCode(ImDrawList* drawList, float x, float y, unsigned int codepoint, ImU32 color, float size = 16.0f,
    float alpha = 1.0f);
void iconCentered(ImDrawList* drawList, const Rect& rect, const char* name, ImU32 color, float size = 16.0f);
void iconCodeCentered(ImDrawList* drawList, const Rect& rect, unsigned int codepoint, ImU32 color, float size = 16.0f);

void fill(ImDrawList* drawList, const Rect& rect, ImU32 color, float radius = 0.0f);
void stroke(ImDrawList* drawList, const Rect& rect, ImU32 color, float radius = 0.0f, float thickness = 1.0f);
void vline(ImDrawList* drawList, float x, float y0, float y1, ImU32 color);
void hline(ImDrawList* drawList, float y, float x0, float x1, ImU32 color);
void triangle(ImDrawList* drawList, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImU32 color);
void chevron(ImDrawList* drawList, float x, float y, float size, bool down, ImU32 color);
void check(ImDrawList* drawList, const Rect& rect, ImU32 color, float thickness = 1.6f);
void dot(ImDrawList* drawList, float cx, float cy, float radius, ImU32 color);
void scrollbar(ImDrawList* drawList, const Rect& track, float offset, float contentHeight, float viewHeight,
    ImU32 color);
void softShadow(ImDrawList* drawList, const Rect& rect, float radius, ImU32 color, float spread = 14.0f,
    float offsetY = 6.0f);

} // namespace ui
} // namespace designer
