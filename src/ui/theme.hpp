#pragma once

#include <imgui.h>

namespace designer
{

struct Palette
{
    ImU32 windowBg;
    ImU32 panel;
    ImU32 panelAlt;
    ImU32 panelRaised;
    ImU32 header;
    ImU32 border;
    ImU32 borderSoft;
    ImU32 text;
    ImU32 textMuted;
    ImU32 textFaint;
    ImU32 accent;
    ImU32 accentHover;
    ImU32 accentSoft;
    ImU32 teal;
    ImU32 green;
    ImU32 red;
    ImU32 yellow;
    ImU32 inputBg;
    ImU32 hover;
    ImU32 selected;
    ImU32 canvas;
    ImU32 canvasGrid;
    ImU32 scrollbar;
};

const Palette& darkPalette();
const Palette& lightPalette();

} // namespace designer
