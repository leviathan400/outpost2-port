#include "dialog_skin.h"

#include <iostream>
#include <vector>

#include "../res/pe_resource.h"
#include "font.h"
#include "ui_font.h"

namespace op2 {

namespace {
Bitmap s_blueprint, s_buttons, s_wide;
bool s_ok = false;

bool loadOne(const PeResourceReader& reader, uint32_t id, Bitmap& out) {
    std::vector<uint8_t> dib = reader.getBitmapDIB(id);
    if (dib.empty() || !out.loadFromDIB(dib.data(), dib.size())) {
        std::cerr << "[DialogSkin] OP2SHRES bitmap " << id << " nao carregou" << std::endl;
        return false;
    }
    return true;
}
} // namespace

bool DialogSkin::load(const PeResourceReader& reader) {
    s_ok = loadOne(reader, 225, s_blueprint) && loadOne(reader, 235, s_buttons) &&
           loadOne(reader, 236, s_wide) && s_blueprint.getWidth() > 0 && s_blueprint.getHeight() > 0;
    return s_ok;
}

bool DialogSkin::ok() { return s_ok; }
const Bitmap& DialogSkin::blueprint() { return s_blueprint; }
const Bitmap& DialogSkin::buttons() { return s_buttons; }
const Bitmap& DialogSkin::wideButtons() { return s_wide; }

// ---------------------------------------------------------------------------

namespace {
std::string noAmp(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&' && i + 1 < s.size()) ++i;   // "&&" -> "&"
        out += s[i];
    }
    return out;
}
constexpr int kFace = UiFont::kDialogText;
// Font puts the top of its 12 px cell at y; Arial −12 has a 1 px taller
// internal leading, so it is drawn 1 px higher to keep the text on its line.
constexpr int kYOffset = -1;
} // namespace

uint32_t SkinText::mapColor(uint32_t c) {
    switch (c | 0xFF000000u) {
    // body text, captions, column headers, values: green
    case 0xFFD8E4F8: case 0xFFCCD6E8: case 0xFFD0DCE8: case 0xFFC8D4E8: case 0xFFE0ECFF:
    case 0xFF9CB0C8: case 0xFFA0B0C8: case 0xFF8AE0AA: case 0xFF65C8FF: case 0xFF8090A8:
    case 0xFF708090: case 0xFFFFFFFF: case 0xFFA8B8D0:
        return DialogSkin::kText;
    // highlight (hover / selected): white
    case 0xFFFFFF99: case 0xFFFFD24A:
        return DialogSkin::kSelText;
    // dimmed (disabled): grey
    case 0xFF60708A: case 0xFF505A6E:
        return DialogSkin::kTextOff;
    default:
        return c;
    }
}

void SkinText::drawRaw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y,
                       uint32_t color) {
    if (!DialogSkin::ok()) { Font::drawText(fb, fbW, fbH, text, x, y, color); return; }
    UiFont::draw(fb, fbW, fbH, noAmp(text), x, y + kYOffset, color, kFace);
}

void SkinText::draw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y,
                    uint32_t color, bool shadow, uint32_t shadowColor) {
    if (!DialogSkin::ok()) {
        Font::drawText(fb, fbW, fbH, text, x, y, color, shadow, shadowColor);
        return;
    }
    UiFont::draw(fb, fbW, fbH, noAmp(text), x, y + kYOffset, mapColor(color), kFace);
}

void SkinText::drawCentered(uint32_t* fb, int fbW, int fbH, const std::string& text, int cx,
                            int y, uint32_t color, bool shadow, uint32_t shadowColor) {
    if (!DialogSkin::ok()) {
        Font::drawTextCentered(fb, fbW, fbH, text, cx, y, color, shadow, shadowColor);
        return;
    }
    const std::string t = noAmp(text);
    UiFont::draw(fb, fbW, fbH, t, cx - UiFont::width(t, kFace) / 2, y + kYOffset,
                 mapColor(color), kFace);
}

int SkinText::width(const std::string& text) {
    return DialogSkin::ok() ? UiFont::width(noAmp(text), kFace) : Font::getTextWidth(text);
}

int SkinText::height() {
    return DialogSkin::ok() ? UiFont::lineHeight(kFace) : Font::getTextHeight();
}

} // namespace op2
