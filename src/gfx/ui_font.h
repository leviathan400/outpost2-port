#pragma once

#include <cstdint>
#include <string>

namespace op2 {

// Fontes da interface do jogo. São TRÊS, não duas:
//   unk_5772A8 (TFrame+0x2828): Arial, lfHeight −11, peso 400 (texto dos painéis)
//   unk_578F08 (TFrame+0x4488): Arial, lfHeight −11, peso 700 (títulos)
//   TFrame+0x14 (sub_49B140):   Arial, lfHeight **+12**, peso 700 — a BARRA DE
//     ESTADO sob a vista. O lfHeight POSITIVO é a altura da CÉLULA (inclui o
//     internal leading), não a do caractere, então esta fonte sai MENOR que a
//     de −11, apesar do número maior. É a fonte que sub_49B360 entrega ao
//     objeto de ajuda (sub_411530 -> [+0xA4], usada por sub_411990) e que
//     sub_499A30 seleciona para desenhar as duas linhas e os contadores de
//     minério.
// LOGFONT: DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, VARIABLE_PITCH|FF_SWISS (0x52).
// sub_415C00 rasteriza cada caractere com GetGlyphOutline(GGO_BITMAP) —
// glifos de 1 bit, sem suavização — e guarda as métricas (tmHeight +0x44,
// ascent +0x48...). O port faz o mesmo com a GDI do Windows. sub_416A00:
// linha = tmHeight; glifo na linha de base (topo + ascent), avanço gmCellIncX.
class UiFont {
public:
    // Qual das três faces. O parâmetro era `bool bold`, e continua aceitando
    // true/false (false = kText, true = kTitle) — por isso nenhum chamador
    // antigo precisou mudar.
    static constexpr int kText = 0;     // Arial −11, 400
    static constexpr int kTitle = 1;    // Arial −11, 700
    static constexpr int kStatus = 2;   // Arial +12, 700 (barra de estado)

    static void draw(uint32_t* fb, int fbW, int fbH, const std::string& text, int x, int y,
                     uint32_t color, int face = kText, int clipL = 0, int clipT = 0, int clipR = 1 << 20,
                     int clipB = 1 << 20);
    static void drawCentered(uint32_t* fb, int fbW, int fbH, const std::string& text, int cx, int y,
                             uint32_t color, int face = kText);
    static int width(const std::string& text, int face = kText);
    static int lineHeight(int face = kText);            // tmHeight
    // y do texto centralizado num botão (sub_40AC80: [+0x24] = (topo + base + 1 −
    // altura do texto) / 2; a altura vem de sub_415EC0 = linhas × tmHeight).
    static int centeredY(int top, int bottom, int face = kText) { return (top + bottom + 1 - lineHeight(face)) / 2; }
    static int ascent(int face = kText);
};

// Diagnóstico (--fonttest): imprime o DPI do DC e as métricas da Arial para
// vários lfHeight, para achar o tamanho cujas métricas são as do original.
void uiFontDumpSizes();

} // namespace op2
