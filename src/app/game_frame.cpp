#include "game_frame.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include "../res/pe_resource.h"

namespace op2 {

// word_4D6988: índice → ID do bitmap no OUT2RES.DLL (lido do OUTPOST2.EXE).
static const int kFrameBitmapIds[GameFrame::kCount] = {
    171, 182, 166, 168, 184, 183, 169, 167, 178, 170, 165, 164, 186,
    181, 179, 192, 188, 189, 190, 191, 187, 208, 207, 211, 210,
};
// Índices (nomes do original pelos endereços dos handles dword_574948 + 4*i).
enum {
    F_TITLE_L = 0,     // 171 140x22
    F_BORDER_L = 1,    // 182 12x378
    F_TITLE_TILE = 2,  // 166 32x22
    F_TITLE_R = 3,     // 168 468x22
    F_SEP_V = 4,       // 184 8x378
    F_BORDER_R = 5,    // 183 12x378
    F_BOTTOM_L = 6,    // 169 140x48
    F_BOTTOM_TILE = 7, // 167 32x42
    F_SEP_V_TILE = 8,  // 178 10x32
    F_BOTTOM_R = 9,    // 170 468x48
    F_BORDER_R_TILE = 10,  // 165 12x32
    F_BORDER_L_TILE = 11,  // 164 12x32
    F_SEP_H = 12,      // 186 192x8
    F_LOGO_ON = 13,    // 181 "OUTPOST 2" verde (janela ativa)
    F_LOGO_OFF = 14,   // 179 cinza
};
// dword_4EA240: botões da barra {distância da borda direita, bitmap normal,
// pressionado, ..., comando de sistema}. Minimizar, maximizar, fechar, restaurar.
struct CaptionButton { int fromRight, normal, pressed; };
static const CaptionButton kCaption[4] = {
    {75, 15, 16},   // SC_MINIMIZE  (192/188)
    {53, 17, 18},   // SC_MAXIMIZE  (189/190)
    {29, 19, 20},   // SC_CLOSE     (191/187)
    {53, 21, 22},   // SC_RESTORE   (208/207)
};

bool GameFrame::load() {                                         // sub_499820
    PeResourceReader reader;
    if (!reader.load("OUT2RES.DLL")) {
        std::cerr << "[GameFrame] OUT2RES.DLL nao encontrado" << std::endl;
        return false;
    }
    m_ok = true;
    for (int i = 0; i < kCount; ++i) {
        const std::vector<uint8_t> dib = reader.getBitmapDIB(static_cast<uint32_t>(kFrameBitmapIds[i]));
        if (dib.empty() || !m_bmp[i].loadFromDIB(dib.data(), dib.size())) m_ok = false;
    }
    return m_ok;
}

void GameFrame::blit(uint32_t* fb, int fbW, int fbH, int idx, int dx, int dy, int sx, int sy, int w, int h) const {
    if (idx < 0 || idx >= kCount) return;
    const Bitmap& b = m_bmp[idx];
    // BitBlt copia só o que existe na origem (fora dela o DC de memória é preto).
    for (int y = 0; y < h; ++y) {
        const int ty = dy + y, srcY = sy + y;
        if (ty < 0 || ty >= fbH) continue;
        for (int x = 0; x < w; ++x) {
            const int tx = dx + x, srcX = sx + x;
            if (tx < 0 || tx >= fbW) continue;
            if (tx >= m_holeL && tx < m_holeR && ty >= m_holeT && ty < m_holeB) continue;
            fb[ty * fbW + tx] = (srcX < b.getWidth() && srcY < b.getHeight()) ? b.getPixel32(srcX, srcY) : 0xFF000000u;
        }
    }
}

void GameFrame::drawPiece(uint32_t* fb, int fbW, int fbH, int idx, int x, int y) const {
    blit(fb, fbW, fbH, idx, x, y, 0, 0, width(idx), height(idx));
}

void GameFrame::drawNonClient(uint32_t* fb, int fbW, int fbH, int wW, int wH, bool active,
                              int pressed) const {   // sub_499E80
    if (!m_ok) return;
    // O original "encaixa" a largura/altura (vista em múltiplos de 32) e deixa o
    // resto preto: W = ((wW − 0xE0) & ~31) + 0xE0, H = ((wH − 0x40) & ~31) + 0x40.
    // Fork: a vista preenche a janela inteira (GameState::relayout), então a
    // moldura vai até a borda real -- senão as bordas direita e inferior caem
    // por baixo da coluna e da barra de status, e a beira fica preta.
    const int W = wW;
    const int H = wH;
    auto fill = [&](int x0, int y0, int x1, int y1) {
        for (int y = std::max(0, y0); y < std::min(fbH, y1); ++y)
            for (int x = std::max(0, x0); x < std::min(fbW, x1); ++x) fb[y * fbW + x] = 0xFF000000u;
    };
    fill(W, 0, wW, wH);
    fill(0, H, wW, wH);
    // Barra de título: esquerda (logo por cima), ladrilhos e direita.
    blit(fb, fbW, fbH, F_TITLE_L, 0, 0, 0, 0, width(F_TITLE_L), 0x16);
    const int logo = active ? F_LOGO_ON : F_LOGO_OFF;
    blit(fb, fbW, fbH, logo, 0, 0, 0, 0, width(logo), height(logo));
    blit(fb, fbW, fbH, F_BORDER_L, 0, 0x16, 0, 0, 0xC, height(F_BORDER_L));
    const int titleR = W - width(F_TITLE_R);
    for (int x = width(F_TITLE_L);; x += width(F_TITLE_TILE)) {
        blit(fb, fbW, fbH, F_TITLE_TILE, x, 0, 0, 0, width(F_TITLE_TILE), 0x16);
        if (x >= titleR) break;
    }
    blit(fb, fbW, fbH, F_TITLE_R, titleR, 0, 0, 0, width(F_TITLE_R), 0x16);
    // Borda direita (topo) e continuações em ladrilhos de 32.
    const int rx = W - 0xC;
    blit(fb, fbW, fbH, F_BORDER_R, rx, 0x16, 0, 0, 0xC, height(F_BORDER_R));
    {
        const int yEnd = H - height(F_BOTTOM_L);
        for (int y = height(F_BORDER_L) + 0x16;; y += height(F_BORDER_L_TILE)) {
            blit(fb, fbW, fbH, F_BORDER_L_TILE, 0, y, 0, 0, 0xC, height(F_BORDER_L_TILE));
            if (yEnd <= y) break;
        }
        blit(fb, fbW, fbH, F_BOTTOM_L, 0, yEnd, 0, 0, 0xC, height(F_BOTTOM_L));
    }
    // Borda inferior: canto esquerdo, ladrilhos e parte direita.
    const int by = H - 0xC;
    blit(fb, fbW, fbH, F_BOTTOM_L, 0xC, by, 0xC, height(F_BOTTOM_L) - 0xC, width(F_BOTTOM_L) - 0xC, 0xC);
    const int bRight = W - width(F_BOTTOM_R);
    for (int x = width(F_BOTTOM_L);; x += width(F_BOTTOM_TILE)) {
        blit(fb, fbW, fbH, F_BOTTOM_TILE, x, by, 0, height(F_BOTTOM_TILE) - 0xC, width(F_BOTTOM_TILE), 0xC);
        if (x >= bRight) break;
    }
    {
        const int yEnd = H - height(F_BOTTOM_R);
        for (int y = height(F_BORDER_R) + 0x16;; y += height(F_BORDER_R_TILE)) {
            blit(fb, fbW, fbH, F_BORDER_R_TILE, rx, y, 0, 0, 0xC, height(F_BORDER_R_TILE));
            if (yEnd <= y) break;
        }
    }
    blit(fb, fbW, fbH, F_BOTTOM_R, bRight, by, 0, height(F_BOTTOM_R) - 0xC, width(F_BOTTOM_R), 0xC);
    blit(fb, fbW, fbH, F_BOTTOM_R, rx, H - height(F_BOTTOM_R), width(F_BOTTOM_R) - 0xC, 0, 0xC, height(F_BOTTOM_R));
    // Botões da barra (dword_56DDB0 == 0: janela; maximizada → "restaurar").
    // PROVISÓRIO: o port roda sempre em janela não maximizada.
    for (int k : {0, 1, 2}) {
        const CaptionButton& c = kCaption[k];
        const int b = (k == pressed) ? c.pressed : c.normal;
        blit(fb, fbW, fbH, b, W - c.fromRight, 1, 0, 0, width(b), height(b));
    }
}

int GameFrame::captionButtonAt(int x, int y, int wW) const {
    if (!m_ok) return -1;
    for (int k : {0, 1, 2}) {
        const CaptionButton& c = kCaption[k];
        const int bx = wW - c.fromRight;
        if (x >= bx && x < bx + width(c.normal) && y >= 1 && y < 1 + height(c.normal)) return k;
    }
    return -1;
}

void GameFrame::drawClient(uint32_t* fb, int fbW, int fbH, int cx, int cy, int cW, int cH,
                           int viewW, int viewH) const {        // sub_499BC0
    if (!m_ok) return;
    m_holeL = cx; m_holeT = cy; m_holeR = cx + viewW; m_holeB = cy + viewH;
    const int R = cW, B = cH;                  // retângulo cliente (relativo)
    auto b = [&](int idx, int dx, int dy, int sx, int sy, int w, int h) {
        blit(fb, fbW, fbH, idx, cx + dx, cy + dy, sx, sy, w, h);
    };
    // Filete vertical entre a vista e a coluna direita (8 px) e faixa sob o minimapa.
    b(F_SEP_V, R - 0xC8, 0, 0, 0, 8, height(F_SEP_V));
    b(F_SEP_H, R - 0xC0, 0x80, 0, 0, 0xC0, 6);
    // Barra de status: canto esquerdo, ladrilhos e parte direita.
    b(F_BOTTOM_L, 0, B - height(F_BOTTOM_L) + 0xC, 0xC, 0, width(F_BOTTOM_L) - 0xC, height(F_BOTTOM_L) - 0xC);
    const int xEnd = R - width(F_BOTTOM_R) + 0xC;
    const int tileH = height(F_BOTTOM_TILE) - 0xC;
    for (int x = width(F_BOTTOM_L) - 0xC;; x += width(F_BOTTOM_TILE)) {
        b(F_BOTTOM_TILE, x, B - height(F_BOTTOM_TILE) + 0xC, 0, 0, width(F_BOTTOM_TILE), tileH);
        if (xEnd <= x) break;
    }
    const int yEnd = B - height(F_BOTTOM_R) + 0xC;
    for (int y = height(F_SEP_V);; y += height(F_SEP_V_TILE)) {
        b(F_SEP_V_TILE, R - 0xC8, y, 0, 0, 8, height(F_SEP_V_TILE));
        if (yEnd <= y) break;
    }
    b(F_BOTTOM_R, R - 0xC8, yEnd, width(F_BOTTOM_R) - 0xD4, 0, 8, height(F_BOTTOM_R) - 0xC);
    b(F_BOTTOM_R, xEnd, B - tileH, 0, height(F_BOTTOM_R) - tileH - 0xC, width(F_BOTTOM_R) - 0xC8, tileH);
    m_holeL = m_holeT = m_holeR = m_holeB = 0;
}

} // namespace op2
