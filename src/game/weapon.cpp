#include "weapon.h"
#include "../audio/sound.h"

#include "command.h"
#include "commands.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "building_sheet.h"
#include "colony.h"
#include "game_map.h"
#include "game_rng.h"
#include "messages.h"
#include "player.h"
#include "structure.h"
#include "tethys_game.h"
#include "terrain_cost.h"
#include "vehicle_anim.h"
#include "disaster.h"
#include "missile.h"
#include "../gfx/artfile.h"

namespace op2 {

static const int kAnimBias = 0x65;   // sub_403E00: `lea eax,[ecx-65h]`

// ---- Classes de arma --------------------------------------------------------
// Por map_id: vt[4] do descritor (+0x250 modo de alcance, +0x254/+0x258 sons),
// vt[31] da unidade (lançamento), vt[6] do descritor (busca de alvo), vt[6] da
// unidade (desenho), vt[35] (animação da explosão) e as tabelas de torre de
// vt[7]/vt[9..10]/vt[8] (Guard Post / Lynx e Panther / Tiger), 16 direções no
// padrão {b, b+15, b+14, ..., b+1}. Valores lidos do OUTPOST2.EXE.
enum Launch { LAUNCH_BALLISTIC /*sub_4A3A90*/, LAUNCH_INSTANT /*sub_4A3E00*/ };
enum Finder { FIND_LOS /*sub_4A6DD0*/, FIND_NEAR /*sub_4A6AA0*/ };
enum DrawKind { DRAW_PROJECTILE /*sub_4A2D50*/, DRAW_LASER /*sub_4A3420*/,
                DRAW_MICROWAVE /*sub_4A3000*/, DRAW_RAILGUN /*sub_4A2A00*/ };
struct WeaponClass {
    int type;
    int rangeMode;          // desc+0x250: 1 = sem linha de visada no teste de alcance
    int fireSound, impactSound;   // desc+0x254 / +0x258
    Launch launch;
    Finder finder;
    DrawKind draw;
    int explodeAnim;        // vt[35] (cru, com viés)
    int leadConst;          // vt[11] do descritor: 4 fixo, ou 0 = max(dist/16+1, 3)
    int turretGP, turretLP, turretTiger;   // bases das tabelas de torre (cru)
};
static const WeaponClass kClasses[] = {
    // LASER: sub_4A58D0 / sub_4A3E00 / sub_4A7020→sub_4A6DD0 / sub_4A3420 / sub_4A2880
    {0x3D, 2, 0xE8, 0xE9, LAUNCH_INSTANT, FIND_LOS, DRAW_LASER, 0x2F6, 4, 0x42B, 0x65A, 0x66A},
    // MICROWAVE: sub_4A5A30 / sub_4A3E00 / sub_4A7040 / sub_4A3000 / sub_4A2880
    {0x3E, 2, 0xEA, 0xEB, LAUNCH_INSTANT, FIND_LOS, DRAW_MICROWAVE, 0x2F6, 4, 0x458, 0x63A, 0x64A},
    // RAIL_GUN: sub_4A5B90 / sub_4A3A90 / sub_4A7000 / sub_4A2A00 / sub_4A2870
    {0x3F, 2, 0xEC, 0xED, LAUNCH_BALLISTIC, FIND_LOS, DRAW_RAILGUN, 0x14B, 0, 0x43B, 0x5B8, 0x5C8},
    // CANNON (RPG): sub_4A5CF0 / sub_4A3A90 / sub_4A6AA0 / sub_4A2D50 / sub_4A2870
    {0x40, 1, 0xF6, 0xF7, LAUNCH_BALLISTIC, FIND_NEAR, DRAW_PROJECTILE, 0x14B, 0, 0x2D5, 0x5D8, 0x5E8},
};

static const WeaponClass* classOf(int type) {
    for (const auto& c : kClasses) if (c.type == type) return &c;
    return nullptr;
}

bool isWeaponProjectile(int type) { return classOf(type) != nullptr; }

const char* weaponDisplayName(int type) {
    const WeaponSheetDesc* d = weaponSheet(type);
    return d ? d->displayName : "";
}

// byte_4E9AF8 / byte_4E9BF8: cos/sen * 64 para as 256 direções.
static const int8_t kCos[256] = {
    64, 63, 63, 63, 63, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60, 59,
    59, 58, 57, 57, 56, 55, 54, 54, 53, 52, 51, 50, 49, 48, 47, 46,
    45, 44, 42, 41, 40, 39, 38, 36, 35, 34, 32, 31, 30, 28, 27, 25,
    24, 23, 21, 20, 18, 17, 15, 14, 12, 10, 9, 7, 6, 4, 3, 1,
    0, -1, -3, -4, -6, -7, -9, -10, -12, -14, -15, -17, -18, -20, -21, -23,
    -24, -25, -27, -28, -30, -31, -32, -34, -35, -36, -38, -39, -40, -41, -42, -44,
    -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -54, -55, -56, -57, -57, -58,
    -59, -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -63, -63, -63, -63,
    -64, -63, -63, -63, -63, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60, -59,
    -59, -58, -57, -57, -56, -55, -54, -54, -53, -52, -51, -50, -49, -48, -47, -46,
    -45, -44, -42, -41, -40, -39, -38, -36, -35, -34, -32, -31, -30, -28, -27, -25,
    -24, -23, -21, -20, -18, -17, -15, -14, -12, -10, -9, -7, -6, -4, -3, -1,
    0, 1, 3, 4, 6, 7, 9, 10, 12, 14, 15, 17, 18, 20, 21, 23,
    24, 25, 27, 28, 30, 31, 32, 34, 35, 36, 38, 39, 40, 41, 42, 44,
    45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 54, 55, 56, 57, 57, 58,
    59, 59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 63, 63, 63, 63,
};
static const int8_t kSin[256] = {
    0, 1, 3, 4, 6, 7, 9, 10, 12, 14, 15, 17, 18, 20, 21, 23,
    24, 25, 27, 28, 30, 31, 32, 34, 35, 36, 38, 39, 40, 41, 42, 44,
    45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 54, 55, 56, 57, 57, 58,
    59, 59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 63, 63, 63, 63,
    64, 63, 63, 63, 63, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60, 59,
    59, 58, 57, 57, 56, 55, 54, 54, 53, 52, 51, 50, 49, 48, 47, 46,
    45, 44, 42, 41, 40, 39, 38, 36, 35, 34, 32, 31, 30, 28, 27, 25,
    24, 23, 21, 20, 18, 17, 15, 14, 12, 10, 9, 7, 6, 4, 3, 1,
    0, -1, -3, -4, -6, -7, -9, -10, -12, -14, -15, -17, -18, -20, -21, -23,
    -24, -25, -27, -28, -30, -31, -32, -34, -35, -36, -38, -39, -40, -41, -42, -44,
    -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -54, -55, -56, -57, -57, -58,
    -59, -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -63, -63, -63, -63,
    -64, -63, -63, -63, -63, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60, -59,
    -59, -58, -57, -57, -56, -55, -54, -54, -53, -52, -51, -50, -49, -48, -47, -46,
    -45, -44, -42, -41, -40, -39, -38, -36, -35, -34, -32, -31, -30, -28, -27, -25,
    -24, -23, -21, -20, -18, -17, -15, -14, -12, -10, -9, -7, -6, -4, -3, -1,
};

// ---- Utilitários --------------------------------------------------------------
static int pxW()      { return g_gameMap.width() * 32; }       // dword_54EBC0
static int pxMask()   { return pxW() - 1; }                    // dword_54EBBC
static int tileMask() { return g_gameMap.width() - 1; }        // dword_54EBC4
static int half(int v) { return v / 2; }                       // cdq; sub; sar 1

// |d| com a volta do mapa em X pela largura (padrão com dword_54EBC0).
static int wrapAbsW(int d) {
    d = std::abs(d);
    if (half(pxW()) < d) d = pxW() - d;
    return d;
}
// Vetor dx com a volta pela MÁSCARA (padrão com dword_54EBBC: ±(W-1)).
static int wrapVecMask(int d) {
    const int h = half(pxMask());
    if (h < std::abs(d)) d = d > 0 ? d - pxMask() : d + pxMask();
    return d;
}
// Distância octogonal: maior + menor/2.
static int octo(int a, int b) { return a < b ? b + half(a) : a + half(b); }

// sub_45C4F0: direção 0..255 de (dx, dy) — atan2 * 256 / (2*pi), truncada.
static uint8_t dirOf(int dx, int dy) {
    static const double kPi = std::acos(-1.0);
    const double v = std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * 256.0 / (kPi * 2.0);
    return static_cast<uint8_t>(static_cast<int>(v));
}

static bool alive(int id) {
    return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1 &&
           !(g_units[id].flags & UNIT_DEAD);
}
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
static int statsOf(const UnitRecord& r) { return (r.owner >> 4) & 0x0F; }   // [+0x1D] sar 4

static const WeapPlayerStats* weapStats(int weaponType, int player) {
    const WeaponSheetDesc* d = weaponSheet(weaponType);
    return (d && player >= 0 && player < 7) ? &d->player[player] : nullptr;
}

// Bloco do descritor do alvo: HP (+0x00) e armadura (+0x08) p/ o nibble alto.
static int maxHPOf(const UnitRecord& r) {
    const int p = statsOf(r);
    if (r.flags & UNIT_VEHICLE) { const VehPlayerStats* v = vehicleSheet(r.type, p); return v ? v->hitPoints : 0; }
    if (r.flags & UNIT_BUILDING) { const StructSheetDesc* d = structSheet(r.type); return d ? d->player[p].hitPoints : 0; }
    return 0;
}
static int armorOf(const UnitRecord& r) {
    const int p = statsOf(r);
    if (r.flags & UNIT_VEHICLE) { const VehPlayerStats* v = vehicleSheet(r.type, p); return v ? v->armor : 256; }
    if (r.flags & UNIT_BUILDING) { const StructSheetDesc* d = structSheet(r.type); return d ? d->player[p].armor : 256; }
    return 256;
}

// sub_404C70: nFrames-1 da animação crua.
static int lastFrameRaw(int raw) {
    const int real = raw - kAnimBias;
    if (!g_art.ok() || real < 0 || real >= g_art.numAnimations()) return 0;
    const int n = static_cast<int>(g_art.animation(real).frames.size());
    return n > 0 ? n - 1 : 0;
}
// sub_404DF0: offsets (x1,y1,x2,y2) do frame — 0 se a animação não tem.
static const int8_t* frameOffsets(int animReal, int frame) {
    static const int8_t kZero[4] = {0, 0, 0, 0};
    if (!g_art.ok() || animReal < 0 || animReal >= g_art.numAnimations()) return kZero;
    const ArtAnimation& a = g_art.animation(animReal);
    if (!a.hasOffsets || frame < 0 || frame >= static_cast<int>(a.frames.size())) return kZero;
    return a.frames[frame].off;
}

bool isTankChassis(int type) { return type == mapLynx || type == mapPanther || type == mapTiger; }

int8_t  dirCos(uint8_t dir) { return kCos[dir]; }
int8_t  dirSin(uint8_t dir) { return kSin[dir]; }
uint8_t dirOfVec(int dx, int dy) { return dirOf(dx, dy); }
int artLastFrameRaw(int raw) { return lastFrameRaw(raw); }
const int8_t* artFrameOffsets(int raw, int frame) { return frameOffsets(raw - kAnimBias, frame); }

static bool isEdenUnit(const UnitRecord& r) {
    const int ow = ownerOf(r);
    return ow < kMaxPlayers && g_playerData[ow].isEden != 0;
}

// vt[7..10] do descritor da arma: animação crua da torre p/ a direção.
static int turretAnimRaw(const WeaponClass& c, int chassis, uint8_t dir) {
    const int base = chassis == mapTiger ? c.turretTiger
                   : (chassis == mapLynx || chassis == mapPanther) ? c.turretLP : c.turretGP;
    const int idx = static_cast<uint8_t>(dir + 8) >> 4;           // add al,8; shr al,2; and ~3
    return idx == 0 ? base : base + 16 - idx;
}

// vt[5] do veículo armado (sub_485600 Lynx / sub_4857E0 Panther / sub_485990
// Tiger): boca da arma = offset do frame 0 da torre (2º par se o projétil/
// unidade `alt` tem 0x4000, só no Tiger) + encaixe do frame 0 do chassi.
static void muzzleOffset(const UnitRecord& shooter, bool alt, uint8_t dir, int& mx, int& my) {
    mx = my = 0;
    const WeaponClass* c = classOf(static_cast<int16_t>(shooter.cargo));
    if (!c || !isTankChassis(shooter.type)) return;
    const int turret = turretAnimRaw(*c, shooter.type, dir) - kAnimBias;
    const int8_t* t = frameOffsets(turret, 0);
    const bool second = alt && shooter.type == mapTiger;           // sub_4A5960: sar 10h
    mx = second ? t[2] : t[0];
    my = second ? t[3] : t[1];
    const int body = vehicleAnimIndex(shooter.type, shooter.rotation, isEdenUnit(shooter));
    const int8_t* b = frameOffsets(body, 0);
    mx += b[0];
    my += b[1];
}

// ---- Linha de visada (sub_436EA0) --------------------------------------------
// Bresenham de (x0,y0) a (x1,y1) em tiles; o ponto de partida e o de chegada não
// são testados. Bloqueia: célula com bit 31 (estrutura/tubo/parede) ou terreno
// com a coluna 5 de dword_4DEB9C != 0 (dword_4DEBB0 = flag de bloqueio de
// visada) — exceto penhascos (8/9) colados à origem ou ao destino. 0 = sem
// visada (inclusive no mesmo tile).
static int lineOfSight(int x0, int y0, int x1, int y1) {
    const int mask = tileMask(), W = g_gameMap.width();
    int dx = x1 - x0;
    const int h = half(mask);
    if (-h > dx) dx = W - x0 + x1;
    else if (h < dx) dx = -((-dx) & mask);
    if (dx == 0 && y1 == y0) return 0;
    const int dy = y1 - y0;
    int major, minor, maj2, min2;                 // var_38/var_44, var_2C/var_28
    int edi, ebx;                                 // eixo maior / menor
    const bool xMajor = std::abs(2 * dy) <= std::abs(2 * dx);
    if (xMajor) { major = dx; minor = dy; maj2 = std::abs(2 * dx); min2 = std::abs(2 * dy); edi = 0; ebx = y0; }
    else        { major = dy; minor = dx; maj2 = std::abs(2 * dy); min2 = std::abs(2 * dx); ebx = 0; edi = y0; }
    const int sMin = minor > 0 ? 1 : -1;          // var_1C
    const int sMaj = major > 0 ? 1 : -1;          // var_20
    int err = min2 - (half(min2) + half(maj2));   // ebp
    int pending = 0;                              // var_14
    auto step = [&]() {
        err += min2;
        if (err > 0) { err -= maj2; ebx += sMin; pending = 1; }
        else if (err == 0) { err = -maj2; ebx += sMin; edi += sMaj; }
        else edi += sMaj;
    };
    if (err > 0) { err -= maj2; ebx += sMin; pending = 1; }
    else if (err == 0) { err = -maj2; ebx += sMin; edi += sMaj; }
    else edi += sMaj;
    int nearStart = 0, nearEnd = 0;               // var_44 / var_38
    auto px = [&]() { return xMajor ? edi : ebx; };   // var_40: x relativo
    auto py = [&]() { return xMajor ? ebx : edi; };   // var_3C: y absoluto
    int cx = px(), cy = py();
    if (pending) { edi += sMaj; --pending; } else step();
    for (;;) {
        if (cx == dx && cy == y1) {
            static const int kRes[4] = {1, -1, 2, 1};             // dword_4D0480
            return kRes[nearStart + nearEnd * 2];
        }
        const uint32_t cell = g_gameMap.rawCell((cx + x0) & mask, cy);
        if (cell & 0x80000000u) return 0;
        const int type = static_cast<int>(cell & 0x1F);
        if (terrainMoveCost(type, 5) != 0) {                    // dword_4DEBB0: bloqueia a visada
            if (type != 8 && type != 9) return 0;
            if (std::abs(cx) <= 1 && std::abs(cy - y0) <= 1) nearStart = 1;
            else if (std::abs(dx - cx) <= 1 && std::abs(cy - y1) <= 1) nearEnd = 1;
            else return 0;
        }
        cx = px(); cy = py();
        if (pending) { edi += sMaj; --pending; } else step();
    }
}

// ---- Alvos -------------------------------------------------------------------
// vt[27]: tile da unidade mais próximo de (sx,sy) — veículo: o próprio tile
// (sub_4018B0); prédio: o tile do footprint mais próximo (sub_483B90).
static void nearestTile(const UnitRecord& t, int sx, int sy, int& ox, int& oy) {
    const int tx = unitOX(t) >> 5, ty = unitOY(t) >> 5;
    if (!(t.flags & UNIT_BUILDING)) { ox = tx; oy = ty; return; }
    const StructSheetDesc* d = structSheet(t.type);
    const int w = d ? d->sizeX : 1, h = d ? d->sizeY : 1;
    const int mask = tileMask(), W = g_gameMap.width();
    // sub_408880: retângulo do footprint.
    const int x1 = (tx - (w >> 1)) & mask;
    int y1 = ty - (h >> 1); if (y1 <= 0) y1 = 0;
    const int x2 = (w + x1 - 1) & mask;
    int y2 = h + y1 - 1; if (y2 > g_gameMap.height() - 1) y2 = g_gameMap.height() - 1;
    int d1 = sx - x1; { const int hh = half(mask); if (d1 < -hh) d1 = W - x1 + sx; else if (d1 > hh) d1 = -((-d1) & mask); } d1 = std::abs(d1);
    int d2 = sx - x2; { const int hh = half(mask); if (d2 < -hh) d2 = W - x2 + sx; else if (d2 > hh) d2 = -((-d2) & mask); } d2 = std::abs(d2);
    if (x2 < x1) {                                  // footprint cruzando a emenda
        if (x1 < sx || x2 > sx) ox = sx;
        else ox = d2 <= d1 ? x2 : x1;
    } else {
        if (x1 <= sx && x2 > sx) ox = sx;
        else ox = d2 <= d1 ? x2 : x1;
    }
    oy = y1 >= sy ? y1 : (y2 <= sy ? y2 : sy);
}

// Prédio-torre ligado à rede (Guard Post: [+0x38]->+0x10) ganha +1 de alcance.
static bool turretBonus(const UnitRecord& u) {
    return (u.flags & UNIT_TURRET_BLD) && u.netConnected;
}

// sub_485140(alvo): o alvo (dword no formato de [+0x4C]) está ao alcance (e à
// vista, se o modo exigir)?
static bool inRangeTo(const UnitRecord& u, uint32_t tgt) {
    const int p = statsOf(u);
    int R;
    if (u.action == 0x25) {
        const VehPlayerStats* v = vehicleSheet(u.type, p);
        R = (v ? v->sight : 0) - 2;                   // desc+0x20 do próprio veículo
        if (R <= 1) R = 1;
    } else {
        const WeapPlayerStats* w = weapStats(static_cast<int16_t>(u.cargo), p);
        R = w ? w->range : 0;
        if (turretBonus(u)) { ++R; if (R >= 7) R = 7; }
    }
    const int sx = unitOX(u) >> 5, sy = unitOY(u) >> 5;
    int tx, ty, dx, dy;
    const int16_t tLo = static_cast<int16_t>(tgt & 0xFFFF), tHi = static_cast<int16_t>(tgt >> 16);
    if ((tgt & 0x80000000u) && tHi != 0x7FFF) {
        const UnitRecord& t = g_units[tLo > 0 && tLo < static_cast<int>(g_units.size()) ? tLo : 0];
        nearestTile(t, sx, sy, tx, ty);
        dy = std::abs(unitOY(u) - ty * 32 - 16);
        const int cx = tx * 32 + 16, ux = unitOX(u);
        dx = (ux > cx ? ux : cx) - (ux < cx ? ux : cx);
    } else {
        tx = tLo; ty = tHi;
        dx = ((tx > sx ? tx : sx) - (tx < sx ? tx : sx)) * 32;
        dy = std::abs(sy - ty) * 32;
    }
    if (half(pxW()) < dx) dx = pxW() - dx;
    const int dist = octo(dx, dy) >> 5;
    const WeaponClass* c = classOf(static_cast<int16_t>(u.cargo));
    const int mode = c ? c->rangeMode : 1;
    if (mode == 1 || dist == 0) return R >= dist;
    if (R < dist) return false;
    return lineOfSight(sx, sy, tx, ty) != 0;
}
static bool inRange(const UnitRecord& u) { return inRangeTo(u, u.target()); }

// Filtro comum das buscas: prédio só se AutoTargeted; máscara de flags; não aliado.
static bool candidateOk(const UnitRecord& c, uint32_t mask, uint32_t ownerBit) {
    if ((c.flags & UNIT_BUILDING) && !(c.flags & UNIT_AUTO_TARGET)) return false;
    if ((c.flags & mask) != mask) return false;
    const int o = ownerOf(c);
    if (o < kMaxPlayers && (g_playerData[o].allyMask & ownerBit)) return false;
    return true;
}

// sub_4A6DD0: varre a lista por Y p/ cima e depois p/ baixo a partir do
// atirador; 1º candidato dentro da caixa e com linha de visada.
static int findTargetLOS(int id, uint32_t mask) {
    const UnitRecord& u = g_units[id];
    const WeapPlayerStats* w = weapStats(static_cast<int16_t>(u.cargo), statsOf(u));
    int R = w ? w->range : 0;                     // min(desc deste, desc da arma) = o mesmo
    if (turretBonus(u)) { ++R; if (R >= 7) R = 7; }
    const int Rpx = R << 5;
    const int sx = unitOX(u), sy = unitOY(u);
    const int xL = sx - Rpx, xR = sx + Rpx;
    const int stx = sx >> 5, sty = sy >> 5;
    const uint32_t bit = 1u << ownerOf(u);
    for (int c = u.lprev; c != 0 && unitOY(g_units[c]) >= sy - Rpx; c = g_units[c].lprev) {
        const UnitRecord& t = g_units[c];
        const int x = unitOX(t);
        if (x <= xL || x >= xR) continue;
        if (!candidateOk(t, mask, bit)) continue;
        if (!lineOfSight(x >> 5, unitOY(t) >> 5, stx, sty)) continue;
        return c;
    }
    for (int c = u.lnext; c != 0 && unitOY(g_units[c]) <= sy + Rpx; c = g_units[c].lnext) {
        const UnitRecord& t = g_units[c];
        const int x = unitOX(t);
        if (x <= xL || x >= xR) continue;
        if (!candidateOk(t, mask, bit)) continue;
        if (!lineOfSight(x >> 5, unitOY(t) >> 5, stx, sty)) continue;
        return c;
    }
    return 0;
}

// sub_4A6AA0 (RPG/Acid): sem linha de visada, distância octogonal < alcance.
// Começa p/ cima ou p/ baixo conforme ((id + tick/32) & 1) e — como no
// original — devolve 0 ao fim da 1ª varredura (o contador começa em 2).
static int findTargetNear(int id, uint32_t mask) {
    const UnitRecord& u = g_units[id];
    const WeapPlayerStats* w = weapStats(static_cast<int16_t>(u.cargo), statsOf(u));
    const int Rfull = w ? w->range : 0;           // var_C
    int R = Rfull;                                // var_18
    if (turretBonus(u)) { ++R; if (R >= 7) R = 7; }
    const int sx = unitOX(u), sy = unitOY(u);
    const int xL = sx - R * 32, xR = sx + R * 32;
    const uint32_t bit = 1u << ownerOf(u);
    auto test = [&](int c) -> bool {
        const UnitRecord& t = g_units[c];
        const int x = unitOX(t);
        if (xL >= x || xR <= x) return false;
        if (!candidateOk(t, mask, bit)) return false;
        const int dx = wrapAbsW(x - sx), dy = std::abs(sy - unitOY(t));
        return (Rfull << 5) > octo(dx, dy);
    };
    const bool up = ((u.id + (TethysGame::tick() >> 5)) & 1) == 0;
    if (up) {
        for (int c = u.lprev; c != 0 && unitOY(g_units[c]) >= sy - R * 32; c = g_units[c].lprev)
            if (test(c)) return c;
    } else {
        for (int c = u.lnext; c != 0 && unitOY(g_units[c]) <= sy + R * 32; c = g_units[c].lnext)
            if (test(c)) return c;
    }
    return 0;
}

static int findTarget(int id, uint32_t mask) {
    const WeaponClass* c = classOf(static_cast<int16_t>(g_units[id].cargo));
    if (!c) return 0;
    return c->finder == FIND_LOS ? findTargetLOS(id, mask) : findTargetNear(id, mask);
}

static void dropRef(int id) {
    if (id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].refCount)
        --g_units[id].refCount;
}
static void clearTarget(UnitRecord& u) {
    if (u.targetIsUnit() && u.hasTarget()) dropRef(u.target4C);
    u.field4E = 0x7FFF;
}
static void setUnitTarget(UnitRecord& u, int t) {
    u.target4C = static_cast<int16_t>(t);
    u.field4E = 0xFFFF;                            // [+0x4C] = id | 0xFFFF0000
    ++g_units[t].refCount;
}

// ---- Disparo (vt[4] do veículo armado = sub_484F80) ---------------------------
static int createProjectile(int type, int x, int y, int owner) {
    // sub_4466B0(..., arg_18=1): centro do tile, y em [16, H*32-16].
    int sx = (x & ~31) + 16;
    int sy = (y & ~31) + 16;
    const int maxY = g_gameMap.height() * 32 - 16;
    if (sy > maxY) sy = maxY;
    if (sy <= 16) sy = 16;
    sx &= pxMask();
    Unit u;
    if (!createUnit(u, type, LOCATION{sx >> 5, sy >> 5}, owner)) return 0;
    UnitRecord& p = g_units[u.id()];
    p.rawPixel = true;
    p.pixelX = sx;
    p.pixelY = sy;
    if (const WeaponSheetDesc* d = weaponSheet(type)) p.flags |= d->ownerFlags | 0x100u;   // desc+0x1F4
    return u.id();
}

static bool fireWeapon(int id) {
    UnitRecord& u = g_units[id];
    if (!inRange(u)) { clearTarget(u); return false; }
    // (0x2000 = atirar no chão: sub_4A1BC0 confere o tile — ordem ainda não portada.)
    const int pid = createProjectile(static_cast<int16_t>(u.cargo), unitOX(u), unitOY(u) + 1, ownerOf(u));
    if (!pid) return false;
    UnitRecord& sh = g_units[id];                  // (createUnit pode realocar o vetor)
    UnitRecord& p = g_units[pid];
    p.owner = static_cast<uint8_t>((p.owner & 0xF6) | 6);       // dono 6, nibble alto = atirador
    sh.flags ^= UNIT_ALT_BARREL;                                // alterna o cano
    p.flags = (p.flags & ~UNIT_ALT_BARREL) | (sh.flags & UNIT_ALT_BARREL);
    p.sourceId = id;                                            // vt[37] = sub_4A27E0
    ++sh.refCount;
    // vt[32] (sub_4A3DD0, alvo unidade) / vt[33] (sub_4A3DA0, alvo lugar).
    p.flags &= ~UNIT_IN_FLIGHT;
    p.field23 = 0; p.field22 = 0x12; p.action = 0x36;
    p.target4C = sh.target4C;
    p.field4E = sh.targetIsUnit() ? 0xFFFF : sh.field4E;
    if (sh.flags & UNIT_FIRE_GROUND) { sh.flags &= ~UNIT_FIRE_GROUND; clearTarget(sh); }
    return true;
}

// sub_4A7D00: sem alvo — a torre passeia (fase (id ^ tick/256) & 15; em
// movimento a fase passa por dword_4EADD8).
static void idleTurret(UnitRecord& u) {
    static const int kMoving[16] = {0, 1, 0, 3, 4, 5, 6, 7, 8, 6, 10, 11, 12, 13, 14, 14};
    int phase = (u.id ^ (TethysGame::tick() >> 8)) & 0x0F;
    if (u.field22 == 1) phase = kMoving[phase];
    const WeapPlayerStats* w = weapStats(static_cast<int16_t>(u.cargo), statsOf(u));
    int target;
    switch (phase) {
        case 0: case 3: case 5: case 7: case 10: case 12: case 14:
            target = u.rotation; break;                           // volta p/ a frente
        case 1: u.turretDir = static_cast<uint8_t>(u.turretDir + (w ? w->turnRate : 0)); return;
        case 8: u.turretDir = static_cast<uint8_t>(u.turretDir - (w ? w->turnRate : 0)); return;
        case 6: case 13: target = TethysGame::getRand(0x100); break;
        default: return;
    }
    // Giro pelo bloco do PRÓPRIO veículo, +0x24 (= Move_Speed nos veículos).
    const VehPlayerStats* v = vehicleSheet(u.type, statsOf(u));
    const int rate = v ? v->moveSpeed : 0;
    const int8_t diff = static_cast<int8_t>(static_cast<uint8_t>(target) - u.turretDir);
    if (std::abs(static_cast<int>(diff)) < rate) u.turretDir = static_cast<uint8_t>(target);
    else if (diff < 0) u.turretDir = static_cast<uint8_t>(u.turretDir - rate);
    else u.turretDir = static_cast<uint8_t>(u.turretDir + rate);
}

void weaponTick(int id) {                                     // sub_4A7AF0 / sub_4A7740
    UnitRecord& u = g_units[id];
    if (!(u.flags & UNIT_VEHICLE)) return;      // Guard Post (vt[4] sub_42CFD0): pendente
    const WeaponClass* c = classOf(static_cast<int16_t>(u.cargo));
    if (!c) return;
    if (u.reload) { --u.reload; return; }
    const WeapPlayerStats* w = weapStats(c->type, statsOf(u));
    auto reload = [&]() {
        UnitRecord& s = g_units[id];
        s.reload = static_cast<int16_t>((w ? w->reload : 0) >> ((s.flags & 0x20) ? 1 : 0));
    };
    if (!u.hasTarget()) {
        if (((u.id ^ TethysGame::tick()) & 7) != 0) return;
        uint32_t mask = UNIT_DAMAGEABLE;
        if (u.field22 == 1) mask |= UNIT_VEHICLE;
        if (u.field4A != 0x7FFF) mask |= UNIT_ARMED;
        const int t = findTarget(id, mask);
        if (t) setUnitTarget(g_units[id], t);
        else idleTurret(g_units[id]);
        return;
    }
    if (u.targetIsUnit()) {
        if (!alive(u.target4C) || (c->rangeMode == 1 && !inRange(u))) {
            u.field4E = 0x7FFF;
            dropRef(u.target4C);
            return;
        }
    } else if (c->rangeMode == 1 && !inRange(u)) {
        return;
    }
    if (fireWeapon(id)) reload();
}

static void addRef(int id) {
    if (id > 0 && id < static_cast<int>(g_units.size())) ++g_units[id].refCount;
}
static void dropIfUnit(uint32_t tgt) { if (tgt & 0x80000000u) dropRef(static_cast<int16_t>(tgt & 0xFFFF)); }

// sub_439810: ponto de rota do alvo — lugar: centro do tile; unidade: o tile
// dela mais próximo da posição (vt[27]).
static uint32_t targetWaypoint(const UnitRecord& u, uint32_t tgt) {
    int tx, ty;
    if (tgt & 0x80000000u) {
        nearestTile(g_units[static_cast<int16_t>(tgt & 0xFFFF)], unitOX(u) >> 5, unitOY(u) >> 5, tx, ty);
    } else {
        tx = static_cast<int16_t>(tgt & 0xFFFF); ty = static_cast<int16_t>(tgt >> 16);
    }
    return packLocation(tx & tileMask(), ty);
}

void unitNearestTile(const UnitRecord& t, int sx, int sy, int& ox, int& oy) { nearestTile(t, sx, sy, ox, oy); }
uint32_t unitTargetWaypoint(const UnitRecord& u, uint32_t tgt) { return targetWaypoint(u, tgt); }

void unitOrderAttack(int id, int type, const std::vector<uint32_t>& wps, uint32_t tgt, int unitIdx, int adj) {   // sub_423850
    UnitRecord& u = g_units[id];
    if ((tgt >> 16) == 0x7FFF) tgt = (tgt & 0xFFFF) | 0xFFFF0000u;
    const bool tUnit = (tgt & 0x80000000u) != 0;
    if (tUnit) {
        const int t = static_cast<int16_t>(tgt & 0xFFFF);
        if (t <= 0 || t >= static_cast<int>(g_units.size()) || g_units[t].id == -1 || (g_units[t].flags & UNIT_DEAD)) {
            u.action = 0; u.field3C = 0; u.field20 = 1;          // alvo morto: cancela
            if (u.orderTarget() & 0x80000000u) { dropRef(u.orderLo); u.field4A = 0x7FFF; }
            if (u.guardTarget() & 0x80000000u) { dropRef(u.guardLo); u.guardHi = 0x7FFF; }
            u.flags &= ~0x400000u;
            return;
        }
    }
    u.field20 = 1;
    u.action = static_cast<uint8_t>(type);
    u.field3C = unitIdx / 4;
    const uint32_t cur = u.orderTarget();
    if (!(u.field4A != 0x7FFF && (cur & 0x80000000u) && tUnit && u.orderLo == static_cast<int16_t>(tgt & 0xFFFF))) {
        dropIfUnit(cur);
        u.setOrderTarget(tgt);
        if (tUnit) addRef(u.orderLo);
    }
    if (type == 0x25) {
        if (u.guardTarget() != tgt) {
            dropIfUnit(u.guardTarget());
            u.guardLo = static_cast<int16_t>(tgt & 0xFFFF); u.guardHi = static_cast<int16_t>(tgt >> 16);
            addRef(u.guardLo);
        }
    } else if (!(u.flags & 0x400000u)) {
        dropIfUnit(u.guardTarget());
        u.guardHi = 0x7FFF;
    }
    if (inRangeTo(u, u.orderTarget()) && wps.empty()) {
        u.hasOrder = false; u.order = OrderBlock{};          // já ao alcance: sem rota
        u.routeDirs.clear(); u.routeIdx = 0;
        if (u.field4E != 0x7FFF && u.target4C == u.orderLo) return;
        if (u.targetIsUnit() && u.hasTarget()) dropRef(u.target4C);
        u.field4E = 0x7FFF;
        if (u.action == 0x25 || (u.flags & 0x400000u)) return;
        u.target4C = u.orderLo; u.field4E = u.field4A;       // [+0x4C] = [+0x48]
        if (u.targetIsUnit()) addRef(u.target4C);
        return;
    }
    // Rota até o alvo; [ordem+0x40] = distância de parada pelo alcance da arma.
    u.hasOrder = true;
    u.order = OrderBlock{};
    u.order.waypoints = wps;
    u.order.count = static_cast<int>(wps.size()) + 1;
    u.order.countMinus1 = static_cast<int>(wps.size());
    const bool moving = tUnit && g_units[u.orderLo].field22 == 1;
    const WeapPlayerStats* w = weapStats(static_cast<int16_t>(u.cargo), statsOf(u));
    const int R = w ? w->range : 0;
    int d;
    if (type == 0x25) d = moving ? R >> 1 : R - 1;
    else d = R - (moving ? 2 : 0) - adj;
    if (d <= 1) d = 1;
    u.order.field40 = d;
    const WeaponClass* wc = classOf(static_cast<int16_t>(u.cargo));
    u.order.multiFlag = wc ? wc->rangeMode : 0;              // [ordem+0x3C] = desc+0x250 (modo do objetivo)
    u.order.waypoints.push_back(targetWaypoint(u, tgt));
    u.routeDirs.clear(); u.routeIdx = 0;
}

void processAttackPacket(int owner, int type, const std::vector<int>& ids,
                         const std::vector<uint32_t>& wps, uint32_t tgt) {   // sub_40E210 cases 36,37
    // (mais de 4 unidades: formação sub_410210 — pendente.)
    bool wallOrBld = false;                                  // var_114 → flag 0x4000000
    if (!(tgt & 0x80000000u)) {
        const int tx = static_cast<int16_t>(tgt & 0xFFFF) & tileMask(), ty = static_cast<int16_t>(tgt >> 16);
        const int ct = static_cast<int>(g_gameMap.rawCell(tx, ty) & 0x1F);
        wallOrBld = ct >= 0x17 && ct <= 0x19;
    } else {
        wallOrBld = (g_units[static_cast<int16_t>(tgt & 0xFFFF)].flags & UNIT_BUILDING) != 0;
    }
    int idx = 0;
    for (int id : ids) {
        UnitRecord& u = g_units[id];
        const int i = idx++;
        if ((u.owner & 0x0F) != owner) continue;
        u.flags = (u.flags & ~0x4000000u) | (wallOrBld ? 0x4000000u : 0u);
        unitResetCombat(id);
        if ((tgt & 0x80000000u) && static_cast<int16_t>(tgt & 0xFFFF) == id) continue;
        const bool cmdable = u.id != -1 && !((u.flags | (u.flags << 2)) & 0x80000u) && u.field22 != 0x0A;
        if ((u.flags & UNIT_ARMED) && cmdable) {
            if (type == 0x24) {
                u.flags &= ~0x400000u;
                if (tgt & 0x80000000u) u.flags &= ~UNIT_FIRE_GROUND;
            }
            unitOrderAttack(id, type, wps, tgt, i, 0);
        }
        // Prédio-torre (0x80) operando: vt[63] — Guard Post ainda não portado.
    }
}

// sub_43ACA0, ramo [+0x20] dos casos 36/37 (loc_43B2B7): com rota, anda; sem, para.
void weaponAttackStart(int id) {
    UnitRecord& u = g_units[id];
    if (!(u.flags & UNIT_ARMED)) { u.field20 = 0; return; }
    if (!u.hasOrder) { u.field22 = 0; u.field20 = 0; }
}

// Fim da ordem de ataque: guarda volta a guardar; senão ação 0 e alvo automático.
static void attackEnded(int id) {
    UnitRecord& u = g_units[id];
    if (u.flags & 0x400000u) unitOrderAttack(id, 0x25, {}, u.guardTarget(), 0, 0);
    else { u.action = 0; u.flags |= UNIT_AUTO_TARGET; }
}

void weaponAttackTick(int id) {                              // sub_43ACA0 jpt_43B312 case 36
    UnitRecord& u = g_units[id];
    const uint32_t ord = u.orderTarget();
    if (!(ord & 0x80000000u)) {                              // lugar
        if (u.flags & 0x4000000u) {
            const int tx = u.orderLo & tileMask(), ty = u.field4A;
            if (!(g_gameMap.rawCell(tx, ty) & 0x80000000u)) {   // a parede sumiu
                u.field4A = 0x7FFF;
                clearTarget(u);
                attackEnded(id);
                return;
            }
        }
        if (!inRangeTo(u, ord)) { u.action = 0; u.flags |= UNIT_AUTO_TARGET; clearTarget(u); return; }
        if (u.target() == ord) return;
        clearTarget(u);
        u.target4C = u.orderLo; u.field4E = u.field4A;
        return;
    }
    const int t = u.orderLo;
    const UnitRecord* tr = (t > 0 && t < static_cast<int>(g_units.size())) ? &g_units[t] : nullptr;
    const int16_t wpn = static_cast<int16_t>(u.cargo);
    if (!tr || tr->id == -1 || (tr->flags & UNIT_DEAD) || !(tr->flags & UNIT_DAMAGEABLE) ||
        ((tr->flags & UNIT_EMP) && wpn == 0x3C) || ((tr->flags & 0x40000u) && wpn == 0x47)) {
        dropRef(t);
        u.field4A = 0x7FFF;
        attackEnded(id);
        return;
    }
    if (u.flags & UNIT_ARMED) {
        if (!inRangeTo(u, ord)) {                            // fora do alcance: refaz a rota
            unitOrderAttack(id, u.action, {}, u.field4E != 0x7FFF ? u.target() : ord, 0, 1);
            return;
        }
        if (u.field4E != 0x7FFF && u.target4C == u.orderLo) {
            if (!(u.flags & 0x400000u)) unitOrderAttack(id, 0x24, {}, ord, 0, 0);
            else unitOrderAttack(id, 0x25, {}, u.guardTarget(), 0, 0);
            return;
        }
        clearTarget(u);
        u.target4C = u.orderLo; u.field4E = u.field4A;
        addRef(u.target4C);
        return;
    }
    if (u.target() == ord || !inRangeTo(u, ord)) return;
    clearTarget(u);
    u.target4C = u.orderLo; u.field4E = u.field4A;
    addRef(u.target4C);
}

void weaponGuardTick(int id) {                               // sub_43ACA0 jpt_43B312 case 37
    UnitRecord& u = g_units[id];
    u.flags |= 0x400000u;
    const int gi = u.guardLo;
    const bool gUnit = (u.guardTarget() & 0x80000000u) != 0;
    const bool gAlive = gi > 0 && gi < static_cast<int>(g_units.size()) && g_units[gi].id != -1 &&
                        !(g_units[gi].flags & UNIT_DEAD);
    if (!gAlive || !gUnit) {                                  // guarda desfeita
        if (!gAlive && gUnit) { dropRef(gi); u.guardHi = 0x7FFF; }
        if (u.orderTarget() & 0x80000000u) { dropRef(u.orderLo); u.field4A = 0x7FFF; }
        u.flags &= ~0x400000u;
        u.action = 0;
        return;
    }
    UnitRecord& g = g_units[gi];
    if (g.lastAttacker && TethysGame::tick() - g.lastAttackTick < 0x96) {
        const int ai = g.lastAttacker;
        const UnitRecord& a = g_units[ai];
        if (a.id != -1 && !(a.flags & UNIT_DEAD)) {
            const int ao = ownerOf(a);
            if (ao < kMaxPlayers && (g_playerData[ao].allyMask & (1u << ownerOf(u)))) return;
            unitOrderAttack(id, 0x24, {}, static_cast<uint32_t>(ai) | 0xFFFF0000u, 0, 1);
            return;
        }
        dropRef(ai);                                          // atacante morto
        g.lastAttacker = 0;
        u.field22 = SUB_WAIT; u.field3C = 4;
        return;
    }
    if (inRangeTo(u, u.orderTarget())) {                      // perto do guardado: espera
        u.field22 = SUB_WAIT; u.field3C = 0x28;
        if (g.lastAttacker) { dropRef(g.lastAttacker); g.lastAttacker = 0; }
        return;
    }
    unitOrderAttack(id, 0x25, {}, u.guardTarget(), 0, 0);    // longe: vai até ele
    u.field3C = 0x28;
}

void processGuardTarget(int me, const std::vector<int>& sel, int tileX, int tileY) {   // sub_4534F0
    std::vector<int> ids;
    for (int id : sel)
        if ((g_units[id].owner & 0x0F) == me && (g_units[id].flags & UNIT_ARMED)) ids.push_back(id);
    // Alvo: unidade sob o clique se o dono dela é aliado do jogador; senão 0.
    uint32_t tgt = 0xFFFF0000u;
    const int occ = g_gameMap.cellOccupant(tileX & tileMask(), tileY);
    if (occ > 0 && occ < static_cast<int>(g_units.size()) && g_units[occ].id != -1) {
        const int ow = ownerOf(g_units[occ]);
        if (ow < kMaxPlayers && (g_playerData[ow].allyMask & (1u << me))) tgt = static_cast<uint32_t>(occ) | 0xFFFF0000u;
    }
    if (!ids.empty()) processAttackPacket(me, 0x25, ids, {}, tgt);
}

void weaponAutoEngage(int id) {                               // sub_485340
    UnitRecord& u = g_units[id];
    const WeaponClass* c = classOf(static_cast<int16_t>(u.cargo));
    if (!c) return;
    // (atacado nos últimos 12 ticks → sub_44DE10 — pendente.)
    if (!u.hasTarget() && (u.flags & UNIT_AUTO_TARGET)) {
        const int16_t wpn = static_cast<int16_t>(u.cargo);
        if (wpn == 0x41 || wpn == 0x42) return;              // autodestruição: nunca procura
        bool retaliate = u.action == 0 && u.field22 == 0;
        if (!retaliate && u.action == 0x25 && (u.field22 == 0 || u.field22 == 6)) {
            const int g = u.guardLo;
            retaliate = g > 0 && g < static_cast<int>(g_units.size()) && g_units[g].lastAttacker == 0;
        }
        // Contra-ataque: quem atacou nos últimos 150 ticks (não aliado) vira alvo
        // de uma ordem de ataque (0x24) — persegue.
        if (retaliate && u.lastAttacker && TethysGame::tick() - u.lastAttackTick < 0x96) {
            const UnitRecord& a = g_units[u.lastAttacker];
            const bool empVsScorpion = wpn == 0x3C && a.type == mapScorpion;
            const int ow = ownerOf(u);
            const bool ally = ow < kMaxPlayers && (g_playerData[ow].allyMask & (1u << ownerOf(a)));
            if (!empVsScorpion && !ally) {
                u.action = 0;
                unitOrderAttack(id, 0x24, {}, static_cast<uint32_t>(u.lastAttacker) | 0xFFFF0000u, 0, 0);
                return;
            }
        }
        uint32_t mask = UNIT_DAMAGEABLE;
        const int o = ownerOf(u);
        if (o < kMaxPlayers && g_playerData[o].isHuman) {
            if (u.field22 == 1) mask |= UNIT_VEHICLE;
            if (u.field4A != 0x7FFF) mask |= UNIT_ARMED;
        }
        const int t = findTarget(id, mask);
        if (t) setUnitTarget(g_units[id], t);
        return;
    }
    // Aproximação (loc_485507): atacando e andando, com o alvo da ordem vivo —
    // ao alcance, refaz a ordem (sem rota: para e aponta a arma).
    if (u.action == 0x24 && u.field22 == 1 && (u.orderTarget() & 0x80000000u) && alive(u.orderLo)) {
        if (inRangeTo(u, u.orderTarget())) unitOrderAttack(id, u.action, {}, u.orderTarget(), 0, 0);
        // (alvo que se moveu: re-rota pela posição [ordem+0x50] — pendente)
    }
}

void unitPeriodicTick(int id) {                               // prólogo de sub_43DE50
    UnitRecord& u = g_units[id];
    if (((u.id ^ TethysGame::tick()) & 7) != 0) return;
    // (contadores 0x40000/0x100000 — espuma/ESG — entram com essas armas.)
    if (u.flags & UNIT_EMP) {                                   // loc_43DEA8: [+0x56]
        if (--u.empTicks != 0) return;
        u.flags &= ~UNIT_EMP;
        if (u.field22 == 0x0A) u.field22 = 0;
        if (u.flags & UNIT_BUILDING) { colonyMarkDirty(u.owner & 0x0F); return; }   // dword_56E37C
        if ((u.flags & UNIT_VEHICLE) && u.id != -1 && !(u.flags & UNIT_DEAD)) {     // loc_43DF05
            u.flags |= UNIT_LIGHTS;
            u.action = 0; u.field20 = 1; u.field3C = 0;
            if (u.flags & UNIT_ARMED) weaponAutoEngage(id);  // vt[0x50]
        }
        return;
    }
    if (u.lastAttacker && TethysGame::tick() - u.lastAttackTick > 0x2BC) {
        dropRef(u.lastAttacker);
        u.lastAttacker = 0;
    }
    if (u.id != -1 && !(u.flags & UNIT_DEAD) && disasterPeriodicTick(id)) return;   // vt[20] do desastre
    if (u.id != -1 && !(u.flags & UNIT_DEAD) && missilePeriodicTick(id)) return;    // vt[20] lançamento / EMP_BIG
    if (u.id != -1 && !(u.flags & UNIT_DEAD) && (u.flags & UNIT_VEHICLE) && (u.flags & UNIT_ARMED))
        weaponAutoEngage(id);                                     // vt[20] (sub_485340)
}

// ---- Lançamento ----------------------------------------------------------------
// Dispersão (comum a sub_4A3A90/sub_4A3E00) + empurra o impacto p/ dentro de
// [pixels pulados, alcance].
static void scatterAndClamp(UnitRecord& p, int tx, int ty) {
    int a = std::abs(tx - p.pixelX);
    if (half(pxW()) < a) a = pxW() - a;
    int b = std::abs(ty - p.pixelY);
    const int s = octo(a, b) >> 4;
    p.impactX = TethysGame::getRand(s) + tx;
    p.impactY = TethysGame::getRand(s) + ty;
    p.impactY -= TethysGame::getRand(s);
    p.impactX -= TethysGame::getRand(s);
    p.impactX &= pxMask();
    const WeaponSheetDesc* d = weaponSheet(p.type);
    int minD = d ? d->skipped : 0;
    const WeapPlayerStats* w = weapStats(p.type, statsOf(p));
    int maxD = (w ? w->range : 0) << 5;
    if (p.flags & UNIT_TURRET_BLD) maxD += 32;
    const int maxY = g_gameMap.height() * 32 - 16;
    for (;;) {
        int vx = p.impactX - p.pixelX;
        const int vy = p.impactY - p.pixelY;
        vx = wrapVecMask(vx);
        p.vecX = vx; p.vecY = vy;
        p.rotation = dirOf(vx, vy);
        const int dist = octo(std::abs(vx), std::abs(vy));
        int L;
        if (dist < minD) L = minD;
        else if (dist > maxD) L = maxD;
        else break;
        p.impactX = p.pixelX + (kCos[p.rotation] * L >> 6);
        int iy = p.pixelY + (kSin[p.rotation] * L >> 6);
        if (iy > maxY) iy = maxY;
        if (iy <= 16) iy = 16;
        p.impactY = iy;
        if (dist < minD) minD = 0; else maxD *= 2;
    }
}

static void launch(int pid, int tx, int ty) {
    UnitRecord& p = g_units[pid];
    const WeaponClass* c = classOf(p.type);
    scatterAndClamp(p, tx, ty);
    if (c && c->launch == LAUNCH_INSTANT) { p.field3C = 4; return; }      // sub_4A3E00
    // sub_4A3A90: sai da boca da arma, duração = dist/16 + 1 (mín. 3).
    int mx = 0, my = 0;
    if (p.sourceId > 0 && p.sourceId < static_cast<int>(g_units.size()))
        muzzleOffset(g_units[p.sourceId], (p.flags & UNIT_ALT_BARREL) != 0, p.rotation, mx, my);
    p.pixelX += mx;
    p.pixelY += my;
    int vx = wrapVecMask(p.impactX - p.pixelX);
    const int vy = p.impactY - p.pixelY;
    p.vecX = vx; p.vecY = vy;
    int T = (octo(std::abs(vx), std::abs(vy)) >> 4) + 1;
    if (T <= 3) T = 3;
    p.flightTicks = static_cast<int16_t>(T);
    p.flags |= UNIT_IN_FLIGHT;
    p.field3C = 0; p.field23 = 0; p.field22 = 0x13; p.action = 0x36;
    p.velQX = vx * 4 / T;
    p.velQY = vy * 4 / T;
}

// sub_4A6940: ponto de mira num alvo-unidade; se ele está andando, antecipa
// pela velocidade do passo vezes o tempo de voo (vt[11] da arma do atirador).
static void aimPoint(const UnitRecord& shooter, const UnitRecord& t, int& ax, int& ay) {
    if (t.field22 != 1) { ax = unitOX(t); ay = unitOY(t); return; }
    const int dx = wrapAbsW(unitOX(shooter) - unitOX(t));
    const int dy = std::abs(unitOY(shooter) - unitOY(t));
    const int dist = octo(dx, dy);
    const WeaponClass* c = classOf(static_cast<int16_t>(shooter.cargo));
    int lead = c && c->leadConst ? c->leadConst : 0;
    if (!c || !c->leadConst) { lead = (dist >> 4) + 1; if (lead <= 3) lead = 3; }
    const int32_t px = t.velX * lead, py = t.velY * lead;
    const int cx = ((static_cast<uint32_t>(px) & 0xFFFF) + t.fracX) > 0xFFFF ? 1 : 0;
    const int cy = ((static_cast<uint32_t>(py) & 0xFFFF) + t.fracY) > 0xFFFF ? 1 : 0;
    ax = (unitOX(t) + (px >> 16) + cx) & pxMask();
    ay = unitOY(t) + (py >> 16) + cy;
}

// vt[36]: duração da explosão.
static int explosionDuration(const UnitRecord& p) {
    const WeaponClass* c = classOf(p.type);
    if (c && c->draw == DRAW_RAILGUN)                              // sub_4A29C0
        return lastFrameRaw(0x2F8) + lastFrameRaw(0x2F9) + lastFrameRaw(0x2FA) + 3;
    return lastFrameRaw(c ? c->explodeAnim : 0);                   // sub_4A27C0
}

// ---- Dano (vt[34] = sub_4A35C0) --------------------------------------------------
static void damageMessage(const UnitRecord& t, int attackerPlayer) {
    if (ownerOf(t) != TethysGame::localPlayer()) return;
    if (attackerPlayer != ownerOf(t)) {
        if (t.flags & UNIT_BUILDING) postMessage(unitOX(t), unitOY(t), "Our colony is under attack!", 0x60);
        else postMessage(unitOX(t), unitOY(t), "We are under attack!", 0x61);
    } else {
        postMessage(unitOX(t), unitOY(t), "Damage Warning!", 0x99);
    }
}

static void explode(int pid) {
    const UnitRecord& p = g_units[pid];
    const WeaponSheetDesc* d = weaponSheet(p.type);
    const WeapPlayerStats* w = weapStats(p.type, statsOf(p));
    if (!d || !w) return;
    const int R = d->radius;
    const int rt = (R + 31) >> 5;
    const int ix = p.impactX, iy = p.impactY;
    if (const WeaponClass* c = classOf(p.type)) soundPlayAt(ix, iy, c->impactSound);   // desc+0x258
    const int x0 = (ix >> 5) - rt, x1 = (ix >> 5) + rt;
    int y0 = (iy >> 5) - rt; if (y0 <= 0) y0 = 0;
    int y1 = (iy >> 5) + rt; if (y1 > g_gameMap.height() - 1) y1 = g_gameMap.height() - 1;
    const int attacker = statsOf(p), source = p.sourceId;
    const bool flag80 = (p.flags & UNIT_TURRET_BLD) != 0;
    for (int x = x0; x <= x1; ++x) {
        const int cxp = x * 32 + 16;
        for (int y = y0; y <= y1; ++y) {
            const uint32_t cell = g_gameMap.rawCell(x & tileMask(), y);
            const int occ = static_cast<int>((cell & GameMap::CELL_OCCUPANCY) >> GameMap::CELL_OCC_SHIFT);
            if (!occ) {
                // Paredes/tubos (cellType 0x17..0x19) levam dano por sub_4A23F0 —
                // o sistema de paredes ainda não foi portado.
                continue;
            }
            if (!alive(occ)) continue;
            UnitRecord& t = g_units[occ];
            if (!(t.flags & UNIT_DAMAGEABLE)) continue;
            int dx, dy, Reff;
            if (t.flags & UNIT_BUILDING) {
                dy = std::abs(iy - y * 32 - 16);
                dx = (ix > cxp ? ix : cxp) - (ix < cxp ? ix : cxp);
                Reff = R >= 0x1A ? R : 0x1A;
            } else {
                dy = std::abs(iy - unitOY(t));
                const int tx = unitOX(t);
                dx = (tx > ix ? tx : ix) - (tx < ix ? tx : ix);
                Reff = R;
            }
            if (half(pxW()) < dx) dx = pxW() - dx;
            const int dist = octo(dx, dy);
            if (dist >= Reff) continue;
            int dmg = ((armorOf(t) * w->concussion) >> 8) + w->penetration;
            if (flag80) dmg += dmg >> 1;
            // (dword_56DE64 — trapaça de dano x4 — fica em 0.)
            if (!(t.flags & UNIT_BUILDING)) dmg = (R - dist) * dmg / R;
            else dmg = (10000 - colonyDirtProtection(t)) * dmg / 20000;
            if (dmg <= 0) continue;
            t.damage = static_cast<int16_t>(t.damage + dmg);
            damageMessage(t, attacker);
            const int hp = maxHPOf(t);
            if (hp <= t.damage) {
                t.damage = static_cast<int16_t>(hp);
                t.flags |= UNIT_DEAD;
                killUnit(occ, 5);
            }
            // Crédito ao atirador ([+0x26]/[+0x28]).
            UnitRecord& tt = g_units[occ];
            if (tt.lastAttacker != source) {
                if (tt.lastAttacker) dropRef(tt.lastAttacker);
                if (source > 0 && source < static_cast<int>(g_units.size())) ++g_units[source].refCount;
                tt.lastAttacker = static_cast<int16_t>(source);
            }
            tt.lastAttackTick = TethysGame::tick();
        }
    }
}

// ---- Máquina do projétil ----------------------------------------------------------
// vt[29] do atirador: veículo (sub_44B620) vivo, sem EMP e fora do estado 0xA.
static bool sourceReady(int src) {
    if (src <= 0 || src >= static_cast<int>(g_units.size())) return false;
    const UnitRecord& s = g_units[src];
    if (s.id == -1) return false;
    if (s.flags & UNIT_VEHICLE) {
        if (((s.flags << 2) | s.flags) & UNIT_EMP) return false;   // 0x20000 ou 0x80000
        return s.field22 != 0x0A;
    }
    return !(s.flags & UNIT_DEAD);                                // sub_4018E0
}

void weaponProjectileAction(int id) {                         // case 54
    UnitRecord& p = g_units[id];
    if (p.field23 == 0x12) {
        if (!sourceReady(p.sourceId)) { p.field22 = SUB_REMOVE; p.field3C = 1; return; }
        const UnitRecord& s = g_units[p.sourceId];
        p.pixelX = unitOX(s);
        p.pixelY = unitOY(s);
        int tx, ty;
        if (p.targetIsUnit()) aimPoint(s, g_units[p.target4C], tx, ty);
        else { tx = p.target4C * 32 + 16; ty = static_cast<int16_t>(p.field4E) * 32 + 16; }
        launch(id, tx, ty);
        g_units[id].field22 = 0x13;
        return;
    }
    if (p.field23 == 0x13) {
        if (!sourceReady(p.sourceId)) { p.field22 = SUB_REMOVE; p.field3C = 1; return; }
        if (const WeaponClass* c = classOf(p.type))      // sub_43ACA0: desc+0x254
            soundPlayAt(unitOX(p), unitOY(p), c->fireSound);
        p.field22 = (p.flags & UNIT_IN_FLIGHT) ? 0x0F : SUB_WAIT;
        return;
    }
    if (!p.exploded) {
        p.exploded = 1;
        explode(id);
        UnitRecord& q = g_units[id];
        q.field22 = SUB_WAIT;
        q.field3C = explosionDuration(q);
    } else {
        p.field22 = SUB_REMOVE;
    }
}

// Sub-estados 0x12/0x13 (sub_43DE50 cases 18/19): gira a torre do atirador
// ([+0x2D]) até a direção do projétil ([+0x1C]) pelo Turret Turn Rate da arma.
static void alignTurret(UnitRecord& p, UnitRecord& s) {
    const WeapPlayerStats* w = weapStats(p.type, statsOf(p));
    const int tr = w ? w->turnRate : 1;
    const int8_t diff = static_cast<int8_t>(p.rotation - s.turretDir);
    if (std::abs(static_cast<int>(diff)) < tr) {
        p.turretDir = p.rotation;
        s.turretDir = p.rotation;
        p.field23 = p.field22;
        p.field22 = 0;
    } else if (diff < 0) {
        s.turretDir = static_cast<uint8_t>(s.turretDir - tr);
    } else {
        s.turretDir = static_cast<uint8_t>(s.turretDir + tr);
    }
}

bool weaponProjectileSubstate(int id) {
    UnitRecord& p = g_units[id];
    if (!isWeaponProjectile(p.type)) return false;
    switch (p.field22) {
        case 0x0F: {                                              // voo (case 15)
            const int t = ++p.field3C;
            const int T = p.flightTicks;
            if (T < t) { p.field23 = p.field22; p.field22 = 0; return true; }
            p.pixelX = p.impactX + (t - T) * p.vecX / T;
            p.pixelY = p.impactY + (t - T) * p.vecY / T;
            return true;
        }
        case 0x12: {                                              // mira (case 18)
            if (p.sourceId <= 0 || p.sourceId >= static_cast<int>(g_units.size())) return true;
            UnitRecord& s = g_units[p.sourceId];
            int vx, vy;
            if (!p.targetIsUnit()) {
                vx = p.target4C * 32 - unitOX(s) + 16;
                vy = static_cast<int16_t>(p.field4E) * 32 - unitOY(s) + 16;
            } else {
                const UnitRecord& t = g_units[p.target4C];
                vx = unitOX(t) - unitOX(s);
                vy = unitOY(t) - unitOY(s);
            }
            p.rotation = dirOf(wrapVecMask(vx), vy);
            alignTurret(p, s);
            return true;
        }
        case 0x13: {                                              // alinha (case 19)
            if (p.sourceId <= 0 || p.sourceId >= static_cast<int>(g_units.size())) return true;
            alignTurret(p, g_units[p.sourceId]);
            return true;
        }
        default: return false;
    }
}

void weaponUnitRemoved(int id) {                              // vt[19] (sub_4A4910...)
    UnitRecord& r = g_units[id];
    if ((isWeaponProjectile(r.type) || isEmpBlast(r.type)) && r.sourceId) dropRef(r.sourceId);
    if (r.lastAttacker) { dropRef(r.lastAttacker); r.lastAttacker = 0; }
    if (r.hasTarget() && r.targetIsUnit() && !isWeaponProjectile(r.type)) clearTarget(r);
}

// ---- Morte do veículo (case 47, loc_43D8A3) --------------------------------------
// sub_48BAE0 / sub_48B6F0: marca de queimado no terreno sob o veículo destruído.
static int scorchTile(int tile) {
    const auto& types = g_gameMap.terrainTypes();
    bool any = false;
    for (const auto& t : types) {
        for (int k = 0; k < 3; ++k) {
            const int lo = t.word(0xDE + k * 4), hi = t.word(0xE0 + k * 4);
            if (lo && lo <= tile && hi >= tile) { any = true; break; }
        }
        if (any) break;
    }
    if (!any) return 0;
    int base = 0;
    for (const auto& t : types) {
        if (t.word(0) > tile || t.word(2) < tile) continue;
        int b = t.word(0xDC);
        if (!b) continue;
        bool dozed = false;
        for (const auto& u : types) if (u.word(4) == tile) { dozed = true; break; }
        if (!dozed && tile != 10) b += 3;
        base = b;
    }
    if (base) base += TethysGame::getRand(3);
    return base;
}

int scorchTileFor(int tile) { return scorchTile(tile); }
int mapLineOfSight(int x0, int y0, int x1, int y1) { return lineOfSight(x0, y0, x1, y1); }

void vehicleDeath(int id) {
    UnitRecord& r = g_units[id];
    // vt[35] (sub_..., ex. nullsub no Lynx): descarga/limpeza da classe — pendente.
    const int hp = maxHPOf(r);
    if (hp == r.damage) {
        // Explosão: AMBIENT_ANIM 0x87D (Spider/Scorpion: 0x87C com a cor do dono).
        const bool arach = r.type == mapSpider || r.type == mapScorpion;
        const int eid = createAmbientAnim(r.pixelX, r.pixelY, arach ? 0x87C : 0x87D, 2, 0);
        if (eid && arach) {
            g_units[eid].flags |= UNIT_EFFECT_COLOR;
            g_units[eid].effectColor = ownerOf(g_units[id]);
        }
        const UnitRecord& v = g_units[id];
        const int tx = (unitOX(v) >> 5) & tileMask(), ty = unitOY(v) >> 5;
        const int tile = g_gameMap.getTile(LOCATION{tx, ty});
        const int nt = scorchTile(tile);
        if (nt) g_gameMap.setTileAndCellType(tx, ty, nt, g_gameMap.getCellType(LOCATION{tx, ty}));
    }
    UnitRecord& v = g_units[id];
    if (v.lastAttacker > 0 && v.lastAttacker < static_cast<int>(g_units.size()) &&
        ((g_units[v.lastAttacker].owner ^ v.owner) & 0x0F)) {
        moraleEnemyVehicleKilled(ownerOf(g_units[v.lastAttacker]));   // sub_44FD40
        // (estatísticas 0x1CC/0x1C4 do jogador — pendente.)
    }
    // Som da morte (sub_43ACA0): com atacante 0x4D; senão pela carga — Starflare
    // (0x41) 0x4F / Tiger 0x4B, Supernova (0x42) 0x4B / Tiger 0x4A, outros 0x4E.
    {
        int snd = 0x4E;
        if (v.lastAttacker) snd = 0x4D;
        else if (v.cargo == 0x41) snd = v.type == mapTiger ? 0x4B : 0x4F;
        else if (v.cargo == 0x42) snd = v.type == mapTiger ? 0x4A : 0x4B;
        soundPlayAt(unitOX(v), unitOY(v), snd);
    }
    if (ownerOf(v) == TethysGame::localPlayer() && maxHPOf(v) == v.damage) {
        char msg[100];
        std::snprintf(msg, sizeof(msg), "%s destroyed", unitTitle(v).c_str());
        postMessage(unitOX(v), unitOY(v), msg, 0x68);
    }
    v.field22 = SUB_REMOVE;
}

// ---- Desenho -------------------------------------------------------------------------
static void pushSprite(std::vector<WeaponDrawOp>& ops, int raw, int frame, int x, int y) {
    WeaponDrawOp op; op.kind = WeaponDrawOp::SPRITE;
    op.anim = raw - kAnimBias; op.frame = frame; op.x = x; op.y = y;
    ops.push_back(op);
}

static void muzzleWorld(const UnitRecord& p, int& x, int& y) {
    const int sid = p.sourceId;
    int mx = 0, my = 0;
    const UnitRecord& s = g_units[(sid > 0 && sid < static_cast<int>(g_units.size())) ? sid : 0];
    if (alive(sid)) muzzleOffset(s, (p.flags & UNIT_ALT_BARREL) != 0, s.turretDir, mx, my);
    x = (unitOX(s) + mx) & pxMask();
    y = unitOY(s) + my;
}

void weaponDrawOps(int id, std::vector<WeaponDrawOp>& ops) {
    const UnitRecord& p = g_units[id];
    const WeaponClass* c = classOf(p.type);
    if (!c) return;
    if (p.field22 == 0 || p.field22 >= 0x12) return;
    switch (c->draw) {
        case DRAW_LASER:                                          // sub_4A3420
        case DRAW_MICROWAVE: {                                    // sub_4A3000
            bool beam = !p.exploded || p.field3C > 5;
            if (c->draw == DRAW_MICROWAVE && p.field3C == 2) beam = false;
            if (beam) {
                int mx, my; muzzleWorld(p, mx, my);
                WeaponDrawOp op;
                if (c->draw == DRAW_LASER) {
                    op.kind = WeaponDrawOp::LASER_LINE;
                    op.x = mx; op.y = my; op.x2 = p.impactX; op.y2 = p.impactY;
                } else {
                    op.kind = WeaponDrawOp::MICROWAVE_LINE;
                    op.x = p.impactX; op.y = p.impactY; op.x2 = mx; op.y2 = my;
                    op.phase = (TethysGame::tick() & 0x0F) * 2;
                }
                ops.push_back(op);
            }
            if (p.exploded)
                pushSprite(ops, c->explodeAnim, lastFrameRaw(c->explodeAnim) - p.field3C, p.impactX, p.impactY);
            return;
        }
        case DRAW_PROJECTILE: {                                   // sub_4A2D50
            if (p.exploded) {
                int f = lastFrameRaw(c->explodeAnim) - p.field3C;
                if (f <= 0) f = 0;
                if (p.flightTicks > 0) {
                    int X8 = p.impactX * 8, Y8 = p.impactY * 8;
                    for (int e = f;; ++e) {
                        int lim = f + p.flightTicks * 2;
                        const int cap = lastFrameRaw(0x2C0) * 2;
                        if (lim >= cap) lim = cap;
                        if (lim <= e) break;
                        X8 -= p.velQX; Y8 -= p.velQY;
                        pushSprite(ops, 0x2C0 + e % 3, e / 3, X8 >> 3, Y8 >> 3);
                    }
                }
                if (!(p.flags & 0x40000)) pushSprite(ops, c->explodeAnim, f, p.impactX, p.impactY);
            } else {
                if (p.field3C) pushSprite(ops, 0x2BC, 0x1F - (static_cast<uint8_t>(p.rotation + 4) >> 3),
                                          p.pixelX & pxMask(), p.pixelY);
                int X8 = (p.pixelX & pxMask()) * 8, Y8 = p.pixelY * 8;
                int n = p.field3C * 2;
                const int cap = lastFrameRaw(0x2C0) * 3;
                if (n >= cap) n = cap;
                if (p.flightTicks * 2 < n) n = p.flightTicks * 2;
                for (int b = 0; b < n; ++b) {
                    X8 -= p.velQX; Y8 -= p.velQY;
                    pushSprite(ops, 0x2C0 + b % 3, b / 3, X8 >> 3, Y8 >> 3);
                }
            }
            return;
        }
        case DRAW_RAILGUN: {                                      // sub_4A2A00
            const int l8 = lastFrameRaw(0x2F8) + 1, l9 = lastFrameRaw(0x2F9) + 1, lA = lastFrameRaw(0x2FA) + 1;
            GameRNG rng; rng.setSeed(static_cast<uint32_t>(p.id + 0x3125323));
            if (p.exploded) {
                const int E = c->explodeAnim;
                int X8 = p.impactX * 8, Y8 = p.impactY * 8;
                const int f = explosionDuration(p) - p.field3C;       // var_10
                for (int k = 0; k < p.flightTicks; ++k) {
                    const int e = f + k;
                    const uint32_t rnd = rng.next();
                    X8 -= p.velQX * 2; Y8 -= p.velQY * 2;
                    if (e < l8) pushSprite(ops, 0x2F8, e, X8 >> 3, Y8 >> 3);
                    else if (e - l8 < l9) pushSprite(ops, 0x2F9, static_cast<int>((rnd + static_cast<uint32_t>(e - l8)) % static_cast<uint32_t>(l9)), X8 >> 3, Y8 >> 3);
                    else if (e - l8 - l9 < lA) pushSprite(ops, 0x2FA, e - l8 - l9, X8 >> 3, Y8 >> 3);
                }
                if (lastFrameRaw(E) >= f) pushSprite(ops, E, f, p.impactX, p.impactY);
            } else {
                const int x = p.pixelX & pxMask(), y = p.pixelY;
                pushSprite(ops, 0x389, 0x1F - (static_cast<uint8_t>(p.rotation + 4) >> 3), x, y);
                int X8 = x * 8, Y8 = y * 8;
                int n = p.field3C - 1;
                if (n <= 0) n = 0;
                for (int k = 0; k < n; ++k) {
                    const uint32_t rnd = rng.next();
                    X8 -= p.velQX * 2; Y8 -= p.velQY * 2;
                    if (k < l8) pushSprite(ops, 0x2F8, k, X8 >> 3, Y8 >> 3);
                    else pushSprite(ops, 0x2F9, static_cast<int>((rnd + static_cast<uint32_t>(k - l8)) % static_cast<uint32_t>(l9)), X8 >> 3, Y8 >> 3);
                }
            }
            return;
        }
    }
}

bool tankTurret(const UnitRecord& r, int bodyAnimReal, int bodyFrame,
                int& turretAnimReal, int& offX, int& offY) {
    const WeaponClass* c = classOf(static_cast<int16_t>(r.cargo));
    if (!c || !isTankChassis(r.type)) return false;
    turretAnimReal = turretAnimRaw(*c, r.type, r.turretDir) - kAnimBias;
    const int8_t* o = frameOffsets(bodyAnimReal, bodyFrame);
    offX = o[0];
    offY = o[1];
    return true;
}

} // namespace op2
