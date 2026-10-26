#include "minimap.h"

#include <algorithm>

#include "../game/game_map.h"

namespace op2 {

namespace {
// Divisão em 16.16 como o original a faz (parte inteira e resto separados):
//   ((a % b) << 16) / b + ((a / b) << 16).
int fixDiv(int a, int b) {
    if (b == 0) return 0;
    return ((a % b) << 16) / b + ((a / b) << 16);
}
int internalW() { return g_gameMap.ok() ? g_gameMap.internalWidth() : 1; }   // dword_54EBC8
int mapHeight() { return g_gameMap.ok() ? g_gameMap.height() : 1; }          // dword_54EBD0
int xOff()      { return g_gameMap.ok() ? g_gameMap.internalXOffset() : 0; } // dword_54EBF0
int realW()     { return xOff() ? internalW() / 2 : internalW(); }
int mul16(int a, int scale) { return static_cast<int>((static_cast<long long>(a) * scale) >> 16); }
}

int MiniMap::padY() const { return xOff() ? 0 : (mapHeight() >> 3); }   // dword_54EBD0 >> 3

void MiniMap::computeScales() {
    const int h = mapHeight();
    const int hAdj = xOff() ? h : h + ((h & ~4) >> 2);     // and 0FFFFFFFBh / sar 2
    m_fitX = fixDiv(m_winW, realW());
    m_fitY = fixDiv(m_winH, hAdj);
    // [+0x1050]: o zoom 0 (encaixar) só é permitido quando o mapa inteiro cabe
    // na janela a 1 px por tile.
    m_minZoom = (realW() < m_winW && hAdj < m_winH) ? 0 : 1;
    if (m_zoom < m_minZoom) m_zoom = m_minZoom;
    if (m_zoom) {
        // A escala é o zoom em 16.16, baixada até 0x1000000 dividir por ela
        // deixando resto de no máximo metade (laço loc_48E937).
        int s = m_zoom << 16;
        for (int guard = 0; guard < 0x1000 && s > 1 && (0x1000000 % s) > (s >> 1); ++guard) --s;
        m_scale = s;
    }
}

void MiniMap::configure(int winW, int winH) {
    m_winW = winW;
    m_winH = winH;
    computeScales();
}

void MiniMap::zoomIn() {                                   // sub_48E660
    m_zoom = std::min(m_zoom + 1, 5);
    computeScales();
    m_offX = 0; m_offY = 0;                                // zerados antes do recálculo
}

void MiniMap::zoomOut() {                                  // sub_48E720
    m_zoom = std::max(m_zoom - 1, m_minZoom);
    computeScales();
    m_offX = 0; m_offY = 0;
}

// sub_48E090: deslocamento X. camX em pixels INTERNOS, viewW em pixels.
int MiniMap::offsetX(int camX, int viewW, bool recompute) const {
    const int xoff = xOff(), A = internalW(), winW = m_winW, zoom = m_zoom;
    if (zoom == 0)                                          // loc_48E347
        return xoff ? -(1 + mul16(xoff, m_fitX)) : 0;
    if (!recompute) return m_offX;
    const int scale = scaleX();
    // O mapa inteiro cabe na janela → centraliza (loc_48E313).
    if (realW() * zoom < winW) return winW / 2 - (A / 4 + xoff) * zoom;

    int left  = mul16(camX >> 5, scale);
    int right = mul16((camX + viewW) >> 5, scale);
    if (right == left) return m_offX;
    int off = m_offX;
    // Uma "volta" do mapa (loc_48E16D/48E18C): a largura do mapa em px, nunca
    // menor que a janela.
    const int period = std::max(mul16(A, scale), winW);
    if (xoff == 0) {                                        // loc_48E12B
        if (-off > right) { left += period; right += period; }
        else if (off + left > winW) off = -left;             // loc_48E1CF
    }
    int nleft = -left;                                      // loc_48E1E6
    bool store = true;
    if (off >= nleft) {
        if (off + right + zoom <= winW) store = false;      // cabe: mantém o deslocamento
        else nleft = winW - zoom - right;                   // alinha à direita
    }
    if (store) off = nleft;                                 // loc_48E209
    if (xoff == 0) {                                        // loc_48E20F
        if (-off >= period) off += period;
        return off;
    }
    // Mapa guardado dobrado: o visível não passa das margens (loc_48E2B0).
    const int margin = mul16(xoff, scale);
    if (-off < margin) off = -margin;
    const int rightLimit = mul16(A / 2 + xoff, scale);
    if (winW - off > rightLimit) off = winW - rightLimit;
    return off;
}

// sub_48E390: deslocamento Y. camY/viewH em pixels.
int MiniMap::offsetY(int camY, int viewH, bool recompute) const {
    const int winH = m_winH, zoom = m_zoom, h = mapHeight();
    if (zoom == 0) return 0;                                // loc_48E58D
    if (!recompute) return m_offY;
    const int scale = scaleY(), pad = padY();
    // O mapa inteiro cabe na vertical → centraliza (loc_48E3B7).
    if (h * zoom < winH) return (winH - h * zoom) / 2;

    const int botTile = (camY + viewH) >> 5;
    const int topPx = mul16((camY >> 5) + pad, scale);
    const int botPx = mul16(pad + botTile, scale);
    if (botPx == topPx) return m_offY;

    int off = m_offY;
    int ntop = -topPx;                                      // loc_48E44A
    if (off >= ntop) {
        const int need = mul16(pad + botTile + 1, scale);
        if (off + need > winH) ntop = winH - need;
        else ntop = off;                                    // cabe: segue para o limite
    }
    off = ntop;
    // Limites do mapa (loc_48E4AE): só nos mapas sem margem interna.
    if (xOff() != 0 || (h & ~7) == 0) return off;
    const int padPx = mul16(pad, scale);
    if (padPx > -off) off = -padPx;
    const int bottomLimit = mul16(h + pad, scale);
    if (bottomLimit < winH - off) off = winH - bottomLimit;
    return off;
}

void MiniMap::update(int camX, int camY, int viewW, int viewH) {
    if (!g_gameMap.ok() || m_winW <= 0) return;
    const int icamX = camX + xOff() * 32;                   // o original trabalha em tiles INTERNOS
    // sub_48DB20(1): baixa o zoom até a vista caber na janela (no máximo 10 passos).
    for (int guard = 0; guard < 10 && m_zoom > m_minZoom; ++guard) {
        if (mul16((viewW >> 5) + 1, scaleX()) <= m_winW) break;
        m_zoom = std::max(m_zoom - 1, m_minZoom);
        computeScales();
        m_offX = 0; m_offY = 0;
    }
    for (int guard = 0; guard < 10 && m_zoom > m_minZoom; ++guard) {
        if (mul16((viewH >> 5) + 1, scaleY()) <= m_winH) break;
        m_zoom = std::max(m_zoom - 1, m_minZoom);
        computeScales();
        m_offX = 0; m_offY = 0;
    }
    m_offX = offsetX(icamX, viewW, true);
    m_offY = offsetY(camY, viewH, true);
}

} // namespace op2
