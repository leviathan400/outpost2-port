#include "ui_font.h"

#include <cstring>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "font.h"

namespace op2 {

namespace {

struct Glyph {
    int w = 0, h = 0;          // gmBlackBoxX / gmBlackBoxY
    int ox = 0, oy = 0;        // gmptGlyphOrigin (oy = acima da linha de base)
    int adv = 0;               // gmCellIncX
    std::vector<uint8_t> bits; // 1 = aceso, w*h
};

struct FaceData {
    bool ok = false;
    int height = 13, ascent = 10;
    Glyph g[256];
};

FaceData s_face[2];
bool s_init = false;

#ifdef _WIN32
void buildFace(FaceData& f, int weight) {           // sub_415C00
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;
    LOGFONTA lf{};
    lf.lfHeight = -11;
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;                  // 1
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;          // 7
    lf.lfPitchAndFamily = 0x52;                      // VARIABLE_PITCH | FF_SWISS
    std::strcpy(lf.lfFaceName, "Arial");
    HFONT font = CreateFontIndirectA(&lf);
    if (!font) { DeleteDC(dc); return; }
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICA tm{};
    GetTextMetricsA(dc, &tm);
    f.height = tm.tmHeight;                          // [+0x44]
    f.ascent = tm.tmAscent;                          // [+0x48]
    MAT2 mat{};
    mat.eM11.value = 1; mat.eM22.value = 1;          // identidade
    for (int c = 0x20; c < 256; ++c) {
        GLYPHMETRICS gm{};
        const DWORD size = GetGlyphOutlineA(dc, static_cast<UINT>(c), GGO_BITMAP, &gm, 0, nullptr, &mat);
        Glyph& g = f.g[c];
        g.adv = gm.gmCellIncX;
        if (size == GDI_ERROR) continue;
        g.ox = gm.gmptGlyphOrigin.x;
        g.oy = gm.gmptGlyphOrigin.y;
        if (size == 0) continue;                     // espaço: só o avanço
        std::vector<uint8_t> buf(size);
        if (GetGlyphOutlineA(dc, static_cast<UINT>(c), GGO_BITMAP, &gm, size, buf.data(), &mat) == GDI_ERROR) continue;
        g.w = static_cast<int>(gm.gmBlackBoxX);
        g.h = static_cast<int>(gm.gmBlackBoxY);
        const int stride = ((g.w + 31) / 32) * 4;    // linhas alinhadas a DWORD
        g.bits.assign(static_cast<size_t>(g.w) * g.h, 0);
        for (int y = 0; y < g.h; ++y)
            for (int x = 0; x < g.w; ++x)
                g.bits[y * g.w + x] = (buf[y * stride + (x >> 3)] >> (7 - (x & 7))) & 1;
    }
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    f.ok = true;
}
#endif

void init() {
    if (s_init) return;
    s_init = true;
#ifdef _WIN32
    buildFace(s_face[0], FW_NORMAL);                 // 400
    buildFace(s_face[1], FW_BOLD);                   // 700
#endif
}

const FaceData& face(bool bold) { init(); return s_face[bold ? 1 : 0]; }

} // namespace

int UiFont::lineHeight(bool bold) {
    const FaceData& f = face(bold);
    return f.ok ? f.height : Font::getTextHeight() + 3;
}
int UiFont::ascent(bool bold) { return face(bold).ascent; }

int UiFont::width(const std::string& text, bool bold) {
    const FaceData& f = face(bold);
    if (!f.ok) return Font::getTextWidth(text);
    int w = 0;
    for (unsigned char c : text) w += f.g[c].adv;
    return w;
}

void UiFont::draw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y, uint32_t color,
                  bool bold, int clipL, int clipT, int clipR, int clipB) {
    const FaceData& f = face(bold);
    if (!f.ok) { Font::drawText(fb, fbW, fbH, text, x, y, color, false); return; }
    if (clipR > fbW) clipR = fbW;
    if (clipB > fbH) clipB = fbH;
    if (clipL < 0) clipL = 0;
    if (clipT < 0) clipT = 0;
    const int base = y + f.ascent;
    for (unsigned char c : text) {
        const Glyph& g = f.g[c];
        const int gx = x + g.ox, gy = base - g.oy;
        for (int yy = 0; yy < g.h; ++yy) {
            const int py = gy + yy;
            if (py < clipT || py >= clipB) continue;
            for (int xx = 0; xx < g.w; ++xx) {
                const int px = gx + xx;
                if (px < clipL || px >= clipR || !g.bits[yy * g.w + xx]) continue;
                fb[py * fbW + px] = color;
            }
        }
        x += g.adv;
    }
}

void UiFont::drawCentered(uint32_t* fb, int fbW, int fbH, const std::string& text, int cx, int y,
                          uint32_t color, bool bold) {
    draw(fb, fbW, fbH, text, cx - width(text, bold) / 2, y, color, bold);
}

} // namespace op2
