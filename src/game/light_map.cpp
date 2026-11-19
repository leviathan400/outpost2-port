#include "light_map.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "building_sheet.h"
#include "game_map.h"
#include "tethys_game.h"
#include "unit.h"
#include "../res/res_manager.h"

namespace op2 {

namespace {

// Um byte por tile, com uma coluna e uma linha a mais (o original alarga a
// vista em 1 x 1 durante o preenchimento -- [+0x81C]/[+0x820] -- para o canto
// direito/de baixo do ultimo tile ter de onde ler).
std::vector<uint8_t> s_lit;
int s_w = 0, s_h = 0;
bool s_active = false, s_wrap = false;
int s_camX = 0, s_camY = 0;

uint32_t s_virMask[16][32];
bool s_virTried = false, s_virFromFile = false;

void loadVirMask() {
    if (s_virTried) return;
    s_virTried = true;
    auto st = g_ResManager.openStream("VIRMASK.RAW");
    if (st && st->size() >= 0x800) {
        const uint8_t* d = st->data();
        for (int i = 0; i < 512; ++i) {
            const uint32_t be = (uint32_t(d[i * 4]) << 24) | (uint32_t(d[i * 4 + 1]) << 16) |
                                (uint32_t(d[i * 4 + 2]) << 8) | uint32_t(d[i * 4 + 3]);
            s_virMask[i / 32][i % 32] = ~be;
        }
        s_virFromFile = true;
        return;
    }
    // Sem o arquivo (o original nem abre): cada quarto do tile acompanha o
    // canto dele -- acrescimo do porte, bordas mais quadradas que as do original.
    for (int m = 0; m < 16; ++m)
        for (int r = 0; r < 32; ++r) {
            uint32_t row = 0;
            for (int i = 0; i < 32; ++i) {
                const int canto = r < 16 ? (i < 16 ? 1 : 2) : (i < 16 ? 4 : 8);
                if (m & canto) row |= 0x80000000u >> i;
            }
            s_virMask[m][r] = row;
        }
}

inline int isqrt(int n) { return n <= 0 ? 0 : static_cast<int>(std::sqrt(static_cast<double>(n))); }

// Coluna do mapa de luz para x de tile (com volta, ou fora = -1).
inline int colOf(int x) {
    if (s_wrap) return ((x % s_w) + s_w) % s_w;
    return (x < 0 || x > s_w) ? -1 : x;
}

inline void setLit(int x, int y) {
    if (y < 0 || y > s_h) return;
    const int c = colOf(x);
    if (c < 0) return;
    s_lit[static_cast<size_t>(y) * (s_w + 1) + c] = 1;
}

inline bool litAt(int x, int y) {
    if (y < 0 || y > s_h) return false;
    const int c = colOf(x);
    if (c < 0) return false;
    return s_lit[static_cast<size_t>(y) * (s_w + 1) + c] != 0;
}

} // namespace

const uint32_t* virMaskRows(int mask) { loadVirMask(); return s_virMask[mask & 0xF]; }
bool virMaskFromFile() { loadVirMask(); return s_virFromFile; }

int unitLightRadius(int id) {
    if (id <= 0 || id >= static_cast<int>(g_units.size())) return 0;
    const UnitRecord& u = g_units[id];
    if (u.id == -1 || (u.flags & UNIT_DEAD) || !(u.flags & UNIT_LIGHTS)) return 0;
    if (!(u.flags & (UNIT_VEHICLE | UNIT_BUILDING))) return 0;
    const uint8_t* b = unitStatBlock(u.type, (u.owner >> 4) & 0x0F);   // [+0x1D] sar 4
    if (!b) return 0;
    int32_t sight = 0;
    std::memcpy(&sight, b + 0x18, 4);                                  // desc + p*0x44 + 0x20
    return sight > 0 ? sight : 0;
}

void lightMapBuild(int camX, int camY, int viewW, int viewH, bool wrapX) {
    s_active = false;
    if (!g_gameMap.ok() || TethysGame::daylightEverywhere()) return;    // dword_56DEA4
    s_w = g_gameMap.width();
    s_h = g_gameMap.height();
    s_wrap = wrapX;
    s_camX = camX; s_camY = camY;
    s_lit.assign(static_cast<size_t>(s_w + 1) * (s_h + 1), 0);
    s_active = true;
    // So quem pode tocar a vista: o raio maximo e pequeno (a tabela do original
    // vai ate 9), a margem cobre o circulo de quem esta logo fora da tela.
    const int vx0 = (camX >> 5) - 12, vx1 = ((camX + viewW) >> 5) + 12;
    const int vy0 = (camY >> 5) - 12, vy1 = ((camY + viewH) >> 5) + 12;
    for (int id = 1; id < static_cast<int>(g_units.size()); ++id) {
        const int r = unitLightRadius(id);
        if (!r) continue;
        const UnitRecord& u = g_units[id];
        const int cx = u.pixelX >> 5, cy = u.pixelY >> 5;           // [+0x14] sar 5 / [+0x18] sar 5
        if (cy + r + 1 < vy0 || cy - r > vy1) continue;
        if (!s_wrap && (cx + r + 1 < vx0 || cx - r > vx1)) continue;
        const int r2 = r * r;
        for (int dy = -r; dy <= r; ++dy) {                          // sub_43A200
            const int t = dy > 0 ? dy - 1 : dy;
            const int w = isqrt(r2 - t * t);                        // byte_4E0C68
            for (int x = cx - w; x <= cx + w + 1; ++x) setLit(x, cy + dy);
        }
    }
}

bool lightMapActive() { return s_active; }

int lightTileMask(int tx, int ty) {                                // sub_46FF60
    if (!s_active) return 0xF;
    return (litAt(tx, ty) ? 1 : 0) | (litAt(tx + 1, ty) ? 2 : 0) |
           (litAt(tx, ty + 1) ? 4 : 0) | (litAt(tx + 1, ty + 1) ? 8 : 0);
}

bool lightPixelLit(int wx, int wy) {
    if (!s_active) return true;
    const int tx = wx >> 5, ty = wy >> 5;                           // >> arredonda p/ baixo
    const int m = lightTileMask(tx, ty);
    if (m == 0xF) return true;
    if (m == 0) return false;
    return (virMaskRows(m)[wy & 31] & (0x80000000u >> (wx & 31))) != 0;   // modo 2, 0x586746
}

bool lightScreenPixelLit(int sx, int sy) { return lightPixelLit(s_camX + sx, s_camY + sy); }

} // namespace op2
