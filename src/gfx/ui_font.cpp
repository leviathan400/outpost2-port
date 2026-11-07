#include "ui_font.h"

#include <cstdio>
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

FaceData s_face[5];
bool s_init = false;

#ifdef _WIN32
// Altura e ascent como o GDI da época os calculava: usWinAscent/usWinDescent da
// tabela OS/2 da própria fonte, escalados pelo corpo pedido (|lfHeight| px) e
// arredondados para cima. O GDI de hoje devolve as medidas CONFERIDAS do
// "hinting" (tabela VDMX) e dá, para a Arial −11, tmHeight 14/ascent 11 em vez
// de 13/10 — e 14 quebra o layout do original: sub_416A00 soma
// (alturaForçada − tmHeight)/2 A CADA SEGMENTO da linha, então com a altura
// forçada 0Ch (12) dos painéis esse termo só é 0 — como o original obviamente
// pretendia — se tmHeight for 12 ou 13. A Arial instalada ainda traz os mesmos
// 1854/434 de 1997, então o cálculo reproduz o valor da época (13/10).
bool metricsFromOS2(HDC dc, int px, int& height, int& ascent) {
    auto tag = [](const char* s) {
        return static_cast<DWORD>(static_cast<uint8_t>(s[0])) |
               (static_cast<DWORD>(static_cast<uint8_t>(s[1])) << 8) |
               (static_cast<DWORD>(static_cast<uint8_t>(s[2])) << 16) |
               (static_cast<DWORD>(static_cast<uint8_t>(s[3])) << 24);
    };
    auto be16 = [](const uint8_t* p) { return static_cast<int>((p[0] << 8) | p[1]); };
    uint8_t os2[96]{}, head[56]{};
    if (GetFontData(dc, tag("OS/2"), 0, os2, sizeof(os2)) == GDI_ERROR) return false;
    if (GetFontData(dc, tag("head"), 0, head, sizeof(head)) == GDI_ERROR) return false;
    const int em = be16(head + 18);                  // unitsPerEm
    const int winAsc = be16(os2 + 74), winDesc = be16(os2 + 76);
    if (em <= 0 || winAsc <= 0) return false;
    const auto up = [em, px](int v) { return (v * px + em - 1) / em; };
    ascent = up(winAsc);
    height = ascent + up(winDesc);
    return true;
}

void buildFace(FaceData& f, int weight, int lfHeight) {   // sub_415C00
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return;
    LOGFONTA lf{};
    lf.lfHeight = lfHeight;
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
    int h = 0, a = 0;
    // O ajuste pela tabela OS/2 vale para o corpo pedido em ALTURA DE
    // CARACTERE (lfHeight negativo), que é como a GDI de 1997 media. Com
    // lfHeight positivo quem escolhe o corpo é a GDI, para a célula fechar na
    // altura pedida, e aí as métricas dela são as que valem.
    if (lf.lfHeight < 0 && metricsFromOS2(dc, -lf.lfHeight, h, a)) { f.height = h; f.ascent = a; }
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
    buildFace(s_face[0], FW_NORMAL, -11);            // 400, texto dos painéis
    buildFace(s_face[1], FW_BOLD, -11);              // 700, títulos
    buildFace(s_face[2], FW_BOLD, 12);               // 700, barra de estado (sub_49B140)
    buildFace(s_face[3], FW_BOLD, -14);              // fork: titulo dos dialogos do shell (medido)
    buildFace(s_face[4], FW_NORMAL, -12);            // fork: texto dos dialogos do shell
#endif
}

const FaceData& face(int i) {
    init();
    if (i < 0 || i > 4) i = 0;
    return s_face[i];
}

} // namespace

int UiFont::lineHeight(int faceIdx) {
    const FaceData& f = face(faceIdx);
    return f.ok ? f.height : Font::getTextHeight() + 3;
}
int UiFont::ascent(int faceIdx) { return face(faceIdx).ascent; }

int UiFont::width(const std::string& text, int faceIdx) {
    const FaceData& f = face(faceIdx);
    if (!f.ok) return Font::getTextWidth(text);
    int w = 0;
    for (unsigned char c : text) w += f.g[c].adv;
    return w;
}

void UiFont::draw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y, uint32_t color,
                  int faceIdx, int clipL, int clipT, int clipR, int clipB) {
    const FaceData& f = face(faceIdx);
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

void uiFontDumpSizes() {
#ifdef _WIN32
    HDC dc = CreateCompatibleDC(nullptr);
    std::printf("[FontTest] LOGPIXELSY=%d\n", GetDeviceCaps(dc, LOGPIXELSY));
    for (int h = -8; h >= -16; --h) {
        LOGFONTA lf{};
        lf.lfHeight = h;
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
        lf.lfPitchAndFamily = 0x52;
        std::strcpy(lf.lfFaceName, "Arial");
        HFONT fo = CreateFontIndirectA(&lf);
        HGDIOBJ old = SelectObject(dc, fo);
        TEXTMETRICA tm{};
        GetTextMetricsA(dc, &tm);
        SIZE sz{};
        GetTextExtentPoint32A(dc, "Added to Storage", 16, &sz);
        OUTLINETEXTMETRICA otm{};
        otm.otmSize = sizeof(otm);
        GetOutlineTextMetricsA(dc, sizeof(otm), &otm);
        std::printf("[FontTest] lfHeight=%d tmHeight=%d asc=%d desc=%d intLead=%d | otmAsc=%d otmDesc=%d "
                    "em=%d macAsc=%d macDesc=%d macLead=%d | \"Added to Storage\"=%d\n",
                    h, (int)tm.tmHeight, (int)tm.tmAscent, (int)tm.tmDescent,
                    (int)tm.tmInternalLeading, (int)otm.otmAscent, (int)otm.otmDescent,
                    (int)otm.otmEMSquare, (int)otm.otmMacAscent, (int)otm.otmMacDescent,
                    (int)otm.otmMacLineGap, (int)sz.cx);
        SelectObject(dc, old);
        DeleteObject(fo);
    }
    DeleteDC(dc);
#endif
}

void UiFont::drawCentered(uint32_t* fb, int fbW, int fbH, const std::string& text, int cx, int y,
                          uint32_t color, int faceIdx) {
    draw(fb, fbW, fbH, text, cx - width(text, faceIdx) / 2, y, color, faceIdx);
}

} // namespace op2
