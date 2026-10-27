#include "globe.h"

#include <algorithm>
#include <cmath>

#include "../game/game_map.h"

namespace op2 {

namespace {

constexpr double kPi      = 3.141592653589793;   // dbl_4CFDD8
constexpr double kInv2Pi  = 0.1591549430918953;  // dbl_4CFDD0
constexpr double k16Bit   = 10430.37835047045;   // dbl_4CFDE0 = 65536 / (2*pi)
constexpr double kHalfPi  = 1.570796326794897;   // dbl_4CFE10
constexpr double k2OverPi = 0.6366197723675814;  // dbl_4CFDE8

int mapW()   { return g_gameMap.ok() ? g_gameMap.internalWidth() : 1; }   // dword_54EBC8
int mapH()   { return g_gameMap.ok() ? g_gameMap.height() : 1; }          // dword_54EBD0
int xMask()  { return mapW() * 32 - 1; }                                  // dword_54EBBC

int mul16(int a, int b) { return static_cast<int>((static_cast<long long>(a) * b) >> 16); }

// Escurecimento de um canal de 5 bits: as tabelas byte_585000 / dword_585400 /
// byte_585C00, montadas por sub_586193 (o valor maximo nunca escurece).
int shade5(int c5, int s) {
    if (c5 == 31) return 31;
    return (c5 * std::min(s + 8, 33)) >> 5;
}

} // namespace

void Globe::setSource(const uint16_t* px, int w, int h) {
    if (m_src == px && m_srcW == w && m_srcH == h) return;
    m_src = px; m_srcW = w; m_srcH = h;
    rebuild();
    updateShade();
}

void Globe::setTarget(int w, int h) {
    if (m_dstW == w && m_dstH == h) return;
    m_dstW = w; m_dstH = h;
    m_cx = w / 2; m_cy = h / 2;                 // sub_41FB70 com os -1000
    rebuild();
    updateShade();
}

void Globe::setRadius(int r) {                  // sub_41F970(r, 1)
    if (m_r == r) return;
    m_r = r;
    rebuild();
    updateShade();
}

void Globe::setSun(int sun16) {                 // sub_41F9A0
    m_sun = sun16 & 0xFFFF;
    updateShade();
}

void Globe::setSpinTarget(int rot16) { m_target = rot16 & 0xFFFF; }

// sub_41FBE0: as tabelas da esfera. A superficie de destino define quais linhas
// de tela o circulo ocupa; de cada uma saem a meia-largura, o x inicial, a
// contagem e, por pixel, a longitude (16 bits) e a coluna na textura.
void Globe::rebuild() {
    m_rows = 0;
    m_halfW.clear(); m_count.clear(); m_xstart.clear();
    m_shade.clear(); m_col.clear(); m_lon.clear();
    if (!m_src || m_dstW <= 0 || m_dstH <= 0 || m_r <= 0) return;

    const int R = m_r, cx = m_cx, cy = m_cy;
    int dz = R;                                  // distancia vertical ao centro
    m_top = 0; m_skip = 0; m_rows = m_dstH;
    const int t = cy - R;
    if (t > 0) { m_top = t; m_rows = m_dstH - t; }
    if (R - cy > 0) { m_skip = R - cy; dz = R + t; }

    int xmin = 0, xmax = m_dstW - 1;
    if (cx - R > 0) xmin = cx - R;
    if (xmax > cx + R) xmax = cx + R;
    m_lineW = xmax - xmin + 1;
    m_yStep = (m_srcH << 15) / R;                // [+0x18]: srcH * 32768 / R
    const double colScale = m_srcW * kInv2Pi;

    m_halfW.assign(m_rows, 0);
    m_count.assign(m_rows, 0);
    m_xstart.assign(m_rows, 0);
    m_shade.assign(static_cast<size_t>(m_rows) * m_lineW, 0);
    m_col.assign(static_cast<size_t>(m_rows) * m_lineW, 0);
    m_lon.assign(static_cast<size_t>(m_rows) * m_lineW, 0);

    for (int row = 0; row < m_rows; ++row, --dz) {
        // O original varre as linhas ate o fim da superficie, mesmo passando do
        // circulo: la a raiz de um negativo vira NaN e o __ftol devolve 0 (a
        // parte baixa do "inteiro indefinido" de 64 bits), ou seja meia-largura
        // zero — uma coluna de 1 px saindo do polo. Reproduzido como esta.
        const long long v = 4LL * (static_cast<long long>(R) * R - static_cast<long long>(dz) * dz);
        const int hw = v < 0 ? 0 : static_cast<int>(std::sqrt(static_cast<double>(v))) >> 1;
        m_halfW[row] = hw;

        int x0 = cx - hw; if (x0 <= 0) x0 = 0;
        if (x0 >= m_dstW) continue;
        int x1 = cx + hw; if (x1 >= m_dstW) x1 = m_dstW - 1;
        m_xstart[row] = x0;
        m_count[row] = x1 - x0 + 1;

        const double hw2 = static_cast<double>(hw) * hw;
        uint8_t* col = &m_col[static_cast<size_t>(row) * m_lineW];
        int* lon = &m_lon[static_cast<size_t>(row) * m_lineW];
        for (int x = x0, k = 0; x <= x1; ++x, ++k) {
            const double dx = x - cx;
            const double dy = std::sqrt(std::max(0.0, hw2 - dx * dx));
            const double d = kPi - std::atan2(dy, dx);     // 0 .. pi, da esquerda p/ a direita
            lon[k] = static_cast<int>(d * k16Bit);
            col[k] = static_cast<uint8_t>(static_cast<int>(d * colScale));
        }
    }
}

// sub_41FEE0: o sombreamento de cada pixel a partir da longitude absoluta
// (longitude do pixel + a fase = rotacao - sol).
void Globe::updateShade() {
    m_phase = (m_rot - m_sun) & 0xFFFF;
    m_dither = 0x17;
    if (!m_shaded || m_shade.empty()) return;
    for (int row = 0; row < m_rows; ++row) {
        const int n = m_count[row];
        if (n <= 0) continue;
        uint8_t* sh = &m_shade[static_cast<size_t>(row) * m_lineW];
        const int* lon = &m_lon[static_cast<size_t>(row) * m_lineW];
        const int k = (m_halfW[row] << 4) / m_r;
        for (int i = 0; i < n; ++i) {
            const int a = (lon[i] + m_phase) & 0xFFFF;
            if (a > 0x4000 && a < 0xC000) { sh[i] = 0; continue; }   // lado da noite
            int s = static_cast<int16_t>(a);
            if (s < 0) s = -s;
            int b = ((0x4000 - s) * k) >> 13;
            m_dither = static_cast<uint8_t>(m_dither + 0x71);        // ruido de 1 nivel
            if (m_dither < 0x55) { if (b > 0) --b; }
            else if (m_dither > 0xAF) { if (b < 0x1F) ++b; }
            sh[i] = static_cast<uint8_t>(b);
        }
        sh[n - 1] = static_cast<uint8_t>(sh[n - 1] >> 1);            // bordas pela metade
        sh[0] = static_cast<uint8_t>(sh[0] >> 1);
    }
}

int Globe::srcLineOf(int row) const { return mul16(m_yStep, row) + m_line; }

// sub_41F9E0 (caminho sem o mouse): acelera ate o alvo e para perto dele.
void Globe::tick() {
    const int delta = static_cast<int16_t>(m_target - m_rot);
    if (m_vel == 0) {
        if (delta > 0xB00) m_vel = -1;
        else if (delta < -0xB00) m_vel = 1;
        else return;
    }
    const int step = static_cast<int16_t>(m_target - m_rot) >> 4;
    if (step < 0) {
        m_vel -= 0x80;
        if (m_vel < step) {
            int v = m_vel + 0x80;
            if (v >= 0) v = 0;
            m_vel = (v > step) ? v : step;
        }
    } else if (step > 0) {
        m_vel += 0x80;
        if (m_vel > step) {
            int v = m_vel - 0x80;
            if (v <= 0) v = 0;
            m_vel = (v < step) ? v : step;
        }
    }
    m_rot = (m_rot + m_vel) & 0xFFFF;
    if (std::abs(m_vel) < 0x20 && std::abs(step) < 0x20) m_vel = 0;
}

// sub_420000.
void Globe::draw(uint32_t* dst, int dstStride, int ox, int oy,
                 int viewLeft, int viewTop, int viewRight, int viewBottom,
                 bool dragging) {
    if (!ready()) return;
    updateShade();

    // Coluna da textura onde o globo comeca, pela rotacao ([+0x00], em bytes
    // no original).
    const int colOff = mul16(m_rot & 0xFFFF, m_srcW);

    // Rolagem vertical: a textura desliza para que o retangulo da vista caiba
    // no globo (o trecho com sub_48EBD0 / sub_48EC00). Ali as bordas vem COM a
    // margem somada, que e justamente o que conta em linhas da textura.
    const int pad = (m_srcH - mapH()) / 2;
    const int topPad = viewTop + pad, botPad = viewBottom + pad;
    m_line = mul16(m_yStep, m_skip);
    bool adjust = false;
    if (topPad < m_line) { m_line = topPad; adjust = true; }
    const int last = mul16(m_yStep, m_rows + m_skip - 1);
    if (botPad > last) { m_line = mul16(m_yStep, 1 - m_rows) + botPad; adjust = true; }
    if (adjust) m_skip = static_cast<int>((static_cast<long long>(m_line) << 16) / m_yStep);

    for (int row = 0; row < m_rows; ++row) {
        const int n = m_count[row];
        if (n <= 0) continue;
        const int sl = srcLineOf(row);
        if (sl < 0 || sl >= m_srcH) continue;            // o teste de [+0x10] do original
        const uint16_t* line = m_src + static_cast<size_t>(sl) * m_srcW;
        const uint8_t* col = &m_col[static_cast<size_t>(row) * m_lineW];
        const uint8_t* sh = &m_shade[static_cast<size_t>(row) * m_lineW];
        const int y = oy + m_top + row;
        uint32_t* out = dst + static_cast<size_t>(y) * dstStride + ox + m_xstart[row];
        for (int k = 0; k < n; ++k) {
            // A coluna e um byte: ao passar do fim da linha o original divide o
            // span em sub_420490 e recua uma linha, o que da esta volta.
            int c = colOff + col[k];
            if (c >= m_srcW) c -= m_srcW;
            const uint16_t p = line[c];
            int r5 = (p >> 10) & 31, g5 = (p >> 5) & 31, b5 = p & 31;
            if (m_shaded) {
                const int s = sh[k];
                r5 = shade5(r5, s); g5 = shade5(g5, s); b5 = shade5(b5, s);
            }
            out[k] = 0xFF000000u | (static_cast<uint32_t>(r5 << 3) << 16)
                                 | (static_cast<uint32_t>(g5 << 3) << 8)
                                 | static_cast<uint32_t>(b5 << 3);
        }
    }

    // O retangulo da vista, pixel a pixel sobre a esfera: branco (0x7FFF), ou
    // ciano (0x3FF) enquanto o globo esta sendo arrastado.
    const uint32_t rc = dragging ? 0xFF00FFFFu : 0xFFFFFFFFu;
    auto plot = [&](int wx, int wy) {
        int sx = 0, sy = 0;
        if (!project(wx, wy, sx, sy)) return;
        dst[static_cast<size_t>(oy + sy) * dstStride + ox + sx] = rc;
    };
    for (int tx = viewLeft; tx < viewRight; ++tx) {
        plot(tx << 5, viewTop << 5);
        plot(tx << 5, viewBottom << 5);
    }
    for (int ty = viewTop; ty < viewBottom; ++ty) {
        plot(viewLeft << 5, ty << 5);
        plot(viewRight << 5, ty << 5);
    }
}

// sub_4208B0: posicao do mundo (1/32 de tile) -> pixel do globo.
bool Globe::project(int wx, int wy, int& sx, int& sy) const {
    if (!ready()) return false;
    // Em Y o mapa ocupa 75% do diametro, a partir de 12,5%.
    const int f = static_cast<int>((static_cast<long long>(wy) * 49152) / (mapH() * 32)) + 0x2000;
    const int py = static_cast<int>((static_cast<long long>(f) * m_r * 2) >> 16) - m_skip + m_top;
    sy = py;

    const int dz = m_cy - py;
    const int centre = mul16((m_rot + 0x4000) & 0xFFFF, xMask() + 1);
    const double ang = (static_cast<double>(wx - centre) / (mapW() * 8)) * kHalfPi;
    const double hw = std::sqrt(std::max(0.0, static_cast<double>(m_r) * m_r
                                              - static_cast<double>(dz) * dz));
    int px = static_cast<int>(m_cx + hw * std::sin(ang));
    // Nunca a esquerda do inicio da linha.
    const int row = py - m_top;
    if (row >= 0 && row < m_rows && m_xstart[row] > px) px = m_xstart[row];
    sx = px;
    return px >= 0 && px < m_dstW && py >= 0 && py < m_dstH;
}

// sub_420750: pixel do globo -> posicao do mundo.
bool Globe::unproject(int sx, int sy, int& wx, int& wy) const {
    if (!ready()) return false;
    const double dz = m_cy - sy, dx = sx - m_cx;
    if (std::sqrt(dx * dx + dz * dz) > m_r) return false;
    const double hw = std::sqrt(std::max(1e-9, static_cast<double>(m_r) * m_r - dz * dz));
    double t = dx / hw;
    t = std::max(-1.0, std::min(1.0, t));
    const int centre = mul16((m_rot + 0x4000) & 0xFFFF, xMask() + 1);
    wx = (static_cast<int>(std::asin(t) * k2OverPi * (mapW() * 8)) + centre) & xMask();

    bool ok = true;
    double f = (static_cast<double>(m_skip - m_top + sy) / (2.0 * m_r) - 0.125)
               * 1.333333333333333;
    if (f < 0.0) { f = 0.0; ok = false; }
    if (f > 1.0) { f = 1.0; ok = false; }
    wy = static_cast<int>(f * (mapH() * 32));
    return ok;
}

} // namespace op2
