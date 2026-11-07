#pragma once

#include <cstdint>
#include <string>

#include "bitmap.h"

namespace op2 {

class PeResourceReader;

// Fork: the look of the shell dialogs (menus) in the original game -- the
// "skin" OP2 paints over its Win32 dialogs. All the art comes from OP2SHRES.DLL:
//   225  the 640x480 navy "blueprint", aligned to the screen; each dialog
//        shows the part of it that lies under the dialog
//   235  push-button strip, 5 states of 75x26
//   236  wide push-button strip, 5 states of 146x26 (the main menu buttons)
// Button states in both strips: 0/1 dim red outline (normal), 2 bright red
// outline (focus), 3/4 bright red with an outer glow (pressed).
// Measured from the original main menu: see Renderer::drawDialogFrame/drawButton.
class DialogSkin {
public:
    static bool load(const PeResourceReader& reader);
    // True when the skin is loaded AND switched on ([Game] MenuSkin=1).
    // Every skinned control falls back to the port's own look otherwise.
    static bool ok();
    static void setEnabled(bool on);
    static bool enabled();
    static const Bitmap& blueprint();
    static const Bitmap& buttons();
    static const Bitmap& wideButtons();

    // Colours of the skinned controls (original menus on Windows 10).
    static constexpr uint32_t kText      = 0xFF00FF00;   // static text, list items
    static constexpr uint32_t kTextOff   = 0xFF808080;   // disabled text
    static constexpr uint32_t kSelBg     = 0xFF0078D7;   // list selection
    static constexpr uint32_t kSelText   = 0xFFFFFFFF;
    static constexpr uint32_t kBoxBg     = 0xFF000000;   // list / edit / combo
    static constexpr uint32_t kBoxEdge   = 0xFFE3E3E3;   // their 2 px border
    static constexpr uint32_t kBoxEdgeDk = 0xFFA0A0A0;
};

// Fork: text of the shell dialogs. Same calls as Font (the port's fixed 8 px
// font), so the menu code can switch over line for line. With the skin loaded
// it draws Arial −12 and turns the port's own UI colours into the original's
// (light body text -> green, highlight -> white, dimmed -> grey); colours it
// does not know (an RTF page's own, error red) pass through. Without the skin
// it is just Font.
class SkinText {
public:
    static void draw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y,
                     uint32_t color, bool shadow = false, uint32_t shadowColor = 0xFF000000);
    static void drawCentered(uint32_t* fb, int fbW, int fbH, const std::string& text, int cx,
                             int y, uint32_t color, bool shadow = false,
                             uint32_t shadowColor = 0xFF000000);
    // No colour mapping (selected list rows: white on blue).
    static void drawRaw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y,
                        uint32_t color);
    static int width(const std::string& text);
    static int height();
    static uint32_t mapColor(uint32_t c);
};

} // namespace op2
