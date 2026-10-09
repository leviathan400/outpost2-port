#pragma once

#include <cstdint>
#include <string>

namespace op2 {

// Fontes da interface do jogo — as do TFrame (sub_49AE60 / sub_49ACA0):
//   unk_5772A8 (TFrame+0x2828): Arial, lfHeight −11, peso 400 (texto dos painéis)
//   unk_578F08 (TFrame+0x4488): Arial, lfHeight −11, peso 700 (títulos)
//   (TFrame+0x28: Arial −11 normal, a mesma do texto)
// LOGFONT: DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, VARIABLE_PITCH|FF_SWISS (0x52).
// sub_415C00 rasteriza cada caractere com GetGlyphOutline(GGO_BITMAP) —
// glifos de 1 bit, sem suavização — e guarda as métricas (tmHeight +0x44,
// ascent +0x48...). O port faz o mesmo com a GDI do Windows. sub_416A00:
// linha = tmHeight; glifo na linha de base (topo + ascent), avanço gmCellIncX.
class UiFont {
public:
    static void draw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y,
                     uint32_t color, bool bold = false, int clipL = 0, int clipT = 0, int clipR = 1 << 20,
                     int clipB = 1 << 20);
    static void drawCentered(uint32_t* fb, int fbW, int fbH, const std::string& text, int cx, int y,
                             uint32_t color, bool bold = false);
    static int width(const std::string& text, bool bold = false);
    static int lineHeight(bool bold = false);           // tmHeight
    static int ascent(bool bold = false);
};

} // namespace op2
