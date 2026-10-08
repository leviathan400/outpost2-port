#include "disaster.h"

#include <cstdio>
#include <cstdlib>

#include "building_sheet.h"
#include "colony.h"
#include "game_map.h"
#include "messages.h"
#include "player.h"
#include "research.h"
#include "structure.h"
#include "tethys_game.h"
#include "unit.h"
#include "walls.h"

namespace op2 {

static const int kLightning = 0x4C, kTornado = 0x4D, kEarthquake = 0x4E, kEruption = 0x4F, kMeteor = 0x50;
static const int kAnimBias = 0x65;   // sub_403E00: `lea eax,[ecx-65h]`

// Format[0x201..0x20F] do EXE: Warning / ALERT / DANGER por desastre.
static const char* const kFmtMeteorWarn  = "Warning: Meteor expected at %d,%d";        // 0x201
static const char* const kFmtMeteorAlert = "ALERT: Meteor expected at %d,%d";          // 0x202
static const char* const kFmtMeteorHit   = "DANGER: Meteor detected at %d,%d";         // 0x203
static const char* const kFmtVortexWarn  = "Warning: Vortex expected at %d,%d";        // 0x204
static const char* const kFmtVortexAlert = "ALERT: Vortex expected at %d,%d";          // 0x205
static const char* const kFmtVortexHit   = "DANGER: Vortex detected at %d,%d";         // 0x206
static const char* const kFmtQuakeWarn   = "Warning: Quake expected at %d,%d";         // 0x207
static const char* const kFmtQuakeAlert  = "ALERT: Quake expected at %d,%d";           // 0x208
static const char* const kFmtQuakeHit    = "DANGER: Quake detected at %d,%d";          // 0x209
static const char* const kFmtStormWarn   = "Warning: Storm expected at %d,%d";         // 0x20A
static const char* const kFmtStormAlert  = "ALERT: Storm expected at %d,%d";           // 0x20B
static const char* const kFmtStormHit    = "DANGER: Storm detected at %d,%d";          // 0x20C
static const char* const kFmtLavaWarn    = "Warning: Lava eruption expected at %d,%d"; // 0x20D
static const char* const kFmtLavaAlert   = "ALERT: Lava eruption expected at %d,%d";   // 0x20E
static const char* const kFmtLavaHit     = "DANGER: Lava eruption detected at %d,%d";  // 0x20F

// ---- Geometria INTERNA do engine ---------------------------------------------------
// Os desastres fazem as contas no x interno (mapas estreitos guardados a partir
// de dword_54EBF0 = 32, largura interna dobrada); o port guarda x a partir de 0.
static int xOff()    { return g_gameMap.internalXOffset(); }    // dword_54EBF0
static int iW()      { return g_gameMap.internalWidth(); }      // dword_54EBC8
static int iMask()   { return iW() - 1; }                       // dword_54EBC4
static int iPxW()    { return iW() * 32; }                      // dword_54EBC0
static int iPxMask() { return iPxW() - 1; }                     // dword_54EBBC
static int mapH()    { return g_gameMap.height(); }             // dword_54EBD0
static int toIX(int px) { return px + xOff() * 32; }
static int toPX(int ix) { return ix - xOff() * 32; }
static int ix(const UnitRecord& r) { return toIX(unitOX(r)); }  // [+0x14]
static int iy(const UnitRecord& r) { return unitOY(r); }        // [+0x18]

// Célula pelo tile interno; fora da área do port não há célula (0).
static bool portTile(int tx, int ty, int& px) {
    px = (tx & iMask()) - xOff();
    return px >= 0 && px < g_gameMap.width() && ty >= 0 && ty < mapH();
}
static uint32_t cellAt(int tx, int ty) {
    int px;
    return portTile(tx, ty, px) ? g_gameMap.rawCell(px, ty) : 0u;
}
static void setCellAt(int tx, int ty, uint32_t v) {
    int px;
    if (portTile(tx, ty, px)) g_gameMap.setRawCell(px, ty, v);
}
// sub_435330: troca só o tileIndex (bits 5..15).
static void setTileAt(int tx, int ty, int tile) {
    setCellAt(tx, ty, (cellAt(tx, ty) & 0xFFFF001Fu) | ((static_cast<uint32_t>(tile) << 5) & 0xFFE0u));
}
// Diferença em tiles com a volta pela máscara (padrão com dword_54EBC4).
static int wrapTileD(int d) {
    const int h = iMask() / 2;
    if (-h > d) return d + iW();
    if (h < d) return -((-d) & iMask());
    return d;
}

void setEarthquake(int x, int y, int magnitude) {           // ?SetEarthquake
    if (magnitude == 0) magnitude = 1;
    Unit u;
    createUnit(u, kEarthquake, LOCATION{x, y}, 6, magnitude);   // sub_4466B0(0x4E, .., 6, mag, 0, 1)
}

// Meteoro/raio/tornado andam: guardam o pixel do original (centro do tile).
static int createDrifting(int type, int x, int y, int cargo) {
    Unit u;
    if (!createUnit(u, type, LOCATION{x, y}, 6, cargo)) return 0;
    UnitRecord& r = g_units[u.id()];
    r.rawPixel = true;
    r.pixelX += 16; r.pixelY += 16;
    return u.id();
}

void setMeteor(int x, int y, int size) { createDrifting(kMeteor, x, y, size); }   // ?SetMeteor

void setLightning(int x, int y, int duration, int endX, int endY) {               // ?SetLightning
    const int id = createDrifting(kLightning, x, y, duration);
    if (!id || endX == -1) return;                          // ctor sub_432DF0: +0x64/+0x66 = -1
    g_units[id].dsEndX = static_cast<int16_t>(endX + xOff());
    g_units[id].dsEndY = static_cast<int16_t>(endY);
}

void setTornado(int x, int y, int duration, int endX, int endY, int immediate) {   // ?SetTornado
    const int id = createDrifting(kTornado, x, y, duration);
    if (!id) return;
    UnitRecord& r = g_units[id];
    r.flags |= 0x10000000u;                                 // ctor sub_48F3D0: desenha no passe de sombra
    r.dsEndX = static_cast<int16_t>(endX == -1 ? -1 : endX + xOff());
    r.dsEndY = static_cast<int16_t>(endY);
    if (immediate) r.flags |= 0x8000u | 0x4000u;            // pula os avisos
}

void setEruption(int x, int y, int speed) {                 // ?SetEruption
    Unit u;
    createUnit(u, kEruption, LOCATION{x, y}, 6, speed);
}

bool isDisaster(int type) { return type >= kLightning && type <= kMeteor; }

// Satélite EDWARD (0x58) em órbita (sub_490800: bits 9-12 do +0x08 do jogador).
static bool hasEdward(int player) { return playerSpaceCount(player, 0x58) != 0; }

// Posição exibida (sub_4A7F70 / sub_413790): (x − x1 + 1, y + 1) — nas
// coordenadas do port o x já começa em 0.
static void postAt(const UnitRecord& r, const char* fmt, int sound) {
    char buf[100];
    std::snprintf(buf, sizeof(buf), fmt, (unitOX(r) >> 5) + 1, (unitOY(r) >> 5) + 1);
    postMessage(unitOX(r), unitOY(r), buf, sound);
}

// sub_4A7F70 (vt[31]): 0 enquanto avisa/espera, 1 quando o desastre age.
static bool warningPhases(int id, const char* warn, int warnSnd, const char* alert, int alertSnd) {
    UnitRecord& r = g_units[id];
    if (r.field22 == SUB_WAIT) return false;
    const int local = TethysGame::localPlayer();
    const bool tech = researchHas(local, researchTopicIndex(r.type));
    if (!(r.flags & 0x4000u)) {
        if (hasEdward(local) && tech) postAt(r, warn, warnSnd);
        r.field22 = SUB_WAIT; r.field3C = 0x1F4; r.flags |= 0x4000u;
        return false;
    }
    if (!(r.flags & 0x8000u)) {
        if (hasEdward(local) || tech) postAt(r, alert, alertSnd);
        r.field22 = SUB_WAIT; r.field3C = 0x1F4; r.flags |= 0x8000u;
        return false;
    }
    return true;
}

// sub_44F8E0 p/ os jogadores 0..5: avisado = tem a tecnologia de alerta (desc+0x1E4).
static void disasterMorale(int type) {
    for (int p = 0; p < 6; ++p) moraleDisaster(p, researchHas(p, researchTopicIndex(type)));
}

static int maxHPOf(const UnitRecord& t) {
    const int p = (t.owner >> 4) & 0x0F;
    if (t.flags & UNIT_VEHICLE) { const VehPlayerStats* v = vehicleSheet(t.type, p); return v ? v->hitPoints : 0; }
    if (t.flags & UNIT_BUILDING) { const StructSheetDesc* d = structSheet(t.type); return d ? d->player[p].hitPoints : 0; }
    return 0;
}
static int armorOf(const UnitRecord& t) {
    const int p = (t.owner >> 4) & 0x0F;
    if (t.flags & UNIT_VEHICLE) { const VehPlayerStats* v = vehicleSheet(t.type, p); return v ? v->armor : 256; }
    if (t.flags & UNIT_BUILDING) { const StructSheetDesc* d = structSheet(t.type); return d ? d->player[p].armor : 256; }
    return 256;
}
// Pixel do prédio no original (sub_4369E0): ((tile*2 + (tam & 1)) << 4).
static void origPixel(const UnitRecord& t, int& x, int& y) {
    if (!(t.flags & UNIT_BUILDING)) { x = unitOX(t); y = unitOY(t); return; }
    int w = 1, h = 1;
    if (const StructSheetDesc* d = structSheet(t.type)) { w = d->sizeX; h = d->sizeY; }
    x = ((t.pixelX >> 5) * 2 + (w & 1)) << 4;
    y = ((t.pixelY >> 5) * 2 + (h & 1)) << 4;
}
// Unidade viva e danificável na célula ([+0x04] != -1, sem 0x20000, com 0x400).
static int damageableIn(uint32_t cell) {
    const int occ = static_cast<int>((cell & 0x7FF0000u) >> 16);
    if (!occ || occ >= static_cast<int>(g_units.size())) return 0;
    const UnitRecord& t = g_units[occ];
    if (t.id == -1 || (t.flags & UNIT_DEAD) || !(t.flags & UNIT_DAMAGEABLE)) return 0;
    return occ;
}
// Soma o dano (16 bits); chegando ao máximo: dano = HP, Kill(5) e 0x20000.
static void addDamage(int id, int dmg) {
    UnitRecord& t = g_units[id];
    t.damage = static_cast<int16_t>(t.damage + dmg);
    const int hp = maxHPOf(t);
    if (hp <= t.damage) {
        t.damage = static_cast<int16_t>(hp);
        t.flags |= UNIT_DEAD;
        killUnit(id, 5);
    }
}

// =====================================================================================
// TERREMOTO 0x4E (off_4CFB08)
// =====================================================================================
static void earthquakeTick(int id) {                         // sub_413790
    if (!warningPhases(id, kFmtQuakeWarn, 0x94, kFmtQuakeAlert, 0x95)) return;
    UnitRecord& q = g_units[id];
    const int mag = static_cast<int16_t>(q.cargo);
    if (!(q.flags & 0x2000u)) {
        q.flags |= 0x2000u | 0x10000u;
        q.field3C = mag * 8 - 1;
        q.quakeAmp = mag * 4 + 0x1E;
        q.quakeDir = 0x100 - TethysGame::getRand(0x200);
        // (som 0x50 em (x,y) — sem áudio no port.)
        postAt(q, kFmtQuakeHit, 0x96);
        disasterMorale(kEarthquake);
        return;
    }
    --q.field3C;
    // Dano em área: raio 6*mag tiles, queda linear até 192*mag px.
    const WeaponSheetDesc* wd = weaponSheet(kEarthquake);
    const WeapPlayerStats* w = wd ? &wd->player[(q.owner >> 4) & 0x0F] : nullptr;
    const int R = mag * 6, Rpx = mag * 0xC0;
    const int qx = unitOX(q), qy = unitOY(q);
    const int cx = qx >> 5, cy = qy >> 5;
    int y0 = cy - R; if (y0 <= 0) y0 = 0;
    int y1 = cy + R; if (y1 > g_gameMap.height() - 1) y1 = g_gameMap.height() - 1;
    const int W = g_gameMap.width() * 32, mask = g_gameMap.width() - 1;
    for (int x = cx - R; x <= cx + R && w; ++x) {
        for (int y = y0; y <= y1; ++y) {
            const int occ = g_gameMap.cellOccupant(x & mask, y);
            if (!occ || occ >= static_cast<int>(g_units.size())) continue;
            UnitRecord& t = g_units[occ];
            if (t.id == -1 || (t.flags & UNIT_DEAD) || !(t.flags & UNIT_DAMAGEABLE)) continue;
            int tx, ty;
            origPixel(t, tx, ty);
            if ((t.flags & UNIT_BUILDING) && ((tx >> 5) != (x & mask) || ((ty - 1) >> 5) != y)) continue;
            const int dy = std::abs(qy - ty);
            int dx = (tx > qx ? tx : qx) - (tx < qx ? tx : qx);
            if (W / 2 < dx) dx = W - dx;
            const int dist = (dx < dy ? dx : dy) / 2 + (dx > dy ? dx : dy);
            if (dist >= Rpx) continue;
            int dmg = ((armorOf(t) * w->concussion) >> 8) + w->penetration;
            dmg = (Rpx - dist) * dmg / Rpx;
            if (t.flags & UNIT_BUILDING) dmg = (10000 - colonyDirtProtection(t)) * dmg / 10000;
            if (t.action == 0x12) dmg >>= 1;
            t.damage = static_cast<int16_t>(t.damage + dmg);
            const int hp = maxHPOf(t);
            if (hp <= t.damage) {
                t.damage = static_cast<int16_t>(hp);
                t.flags |= UNIT_DEAD;
                killUnit(occ, 5);
            }
        }
    }
    UnitRecord& qq = g_units[id];
    // (a cada 3 passos, som 0x50.)
    if ((qq.flags & 0x10000u) && qq.field3C == 0) {          // 2ª onda (réplica)
        qq.flags &= ~0x10000u;
        qq.field3C = mag * 8 - 1;
        return;
    }
    if (!(qq.flags & 0x10000u) && qq.field3C == 0) qq.field22 = SUB_REMOVE;
}

// =====================================================================================
// METEORO 0x50 (vt[20] sub_44A210, vt[32] sub_44ACB0, vt[6] sub_44A9A0, vt[9] sub_44ABF0)
// =====================================================================================
// Animação da explosão no fim (Kill): tamanho 2 → 0x5B4, demais → 0x5B3.
static int meteorDieAnim(int size) { return size == 2 ? 0x5B4 : 0x5B3; }

// [+0x3C] == 2: defesa antimeteoro (0x2B) operando a ≤ 20 tiles do alvo, num
// jogador com Observatório e Defesa operando (estatísticas +0x1C/+0x20). A
// chance sobe com o "Improved" do Observatório (byte +0x1D do bloco 0x33).
static void meteorDefense(int id) {
    for (int p = 0; p < TethysGame::noPlayers() && p < kMaxPlayers; ++p) {
        const ColonyState& c = g_colony[p];
        if (!c.stat(ST_OBSERVATORY) || !c.stat(ST_METEOR_DEF)) continue;
        int found = 0, seen = 0;
        for (int b : colonyStructures(p)) {
            if (c.stat(ST_METEOR_DEF) <= seen) break;
            const UnitRecord& d = g_units[b];
            if (d.type != 0x2B || !structureOperational(d)) continue;
            const UnitRecord& m = g_units[id];
            int dxp, dyp;
            origPixel(d, dxp, dyp);
            const int dy = std::abs(m.dsTY - (dyp >> 5));
            const int dx = std::abs(wrapTileD((toIX(dxp) >> 5) - m.dsTX));
            if ((dx < dy ? dx : dy) / 2 + (dx > dy ? dx : dy) <= 20) { found = b; break; }
            ++seen;
        }
        if (!found) continue;
        const uint8_t* obs = unitStatBlock(0x33, g_units[found].owner & 0x0F);
        const int sz = static_cast<int16_t>(g_units[id].cargo);
        int chance = 0;
        if (obs && obs[0x1D]) chance = sz == 0 ? 75 : sz == 1 ? 60 : sz == 2 ? 50 : 0;
        else                  chance = sz == 0 ? 50 : sz == 1 ? 40 : sz == 2 ? 30 : 0;
        UnitRecord& m = g_units[id];
        if (TethysGame::getRand(100) < chance) {
            m.flags |= 0x40000u | 0x10000u;
            m.dsAux = found;
            // sub_439180: grava (x>>5, y>>5) — tiles — no lugar do pixel; som 0x5B.
            postMessage(toPX(ix(m) >> 5), iy(m) >> 5, "Meteor Destroyed!", 0);
            killUnit(id, artLastFrameRaw(meteorDieAnim(sz)));
        } else {
            m.flags |= 0x40000u;
            m.dsAux = found;
        }
    }
}

static void meteorImpact(int id) {
    UnitRecord& m = g_units[id];
    const int sz = static_cast<int16_t>(m.cargo);
    m.flags &= ~0x40000u;
    // (som do impacto: 0x4E / 0x4D / 0x4A pelo tamanho.)
    // Dano base: bloco do jogador 0 do descritor 0x50 (off_4E1480): tamanho 0
    // → +0x0C, 1 → +0x2C, 2 → +0x28.
    const WeaponSheetDesc* wd = weaponSheet(kMeteor);
    int base = 0;
    if (wd) base = sz == 0 ? wd->player[0].commonOre : sz == 1 ? wd->player[0].penetration
                                                          : wd->player[0].concussion;
    const int R = 2 * sz + 1;
    const int tx0 = m.dsTX, ty0 = m.dsTY;
    int y0 = ty0 - R; if (y0 <= 0) y0 = 0;
    int y1 = ty0 + R; if (mapH() - 1 < y1) y1 = mapH() - 1;
    for (int x = tx0 - R; x <= tx0 + R; ++x) {
        for (int y = y0; y <= y1; ++y) {
            const uint32_t cell = cellAt(x, y);
            if (!(cell & 0x7FF0000u)) {
                // Parede (cellType 0x17..0x19): sub_4A23F0(x, y, ebx) — dano em
                // parede ainda não portado.
                continue;
            }
            const int occ = damageableIn(cell);
            if (!occ) continue;
            const UnitRecord& t = g_units[occ];
            int dmg = base;
            if (t.flags & UNIT_BUILDING) dmg = (10000 - colonyDirtProtection(t)) * base / 10000;
            const int dy = std::abs(ty0 - y);
            int dx = std::abs(tx0 - x);
            if (iW() / 2 < dx) dx = iW() - dx;
            const int sh = ((dx < dy ? dx : dy) / 2 + (dx > dy ? dx : dy)) & 0xFF;   // add cl, al
            dmg >>= (sh & 31);
            addDamage(occ, dmg);
        }
    }
    // Cratera: tile queimado no centro (sub_48BAE0 / sub_48B6F0).
    const int tile = static_cast<int>((cellAt(tx0, ty0) & 0xFFE0u) >> 5);
    const int nt = scorchTileFor(tile);
    if (nt) {
        setTileAt(tx0, ty0, nt);
        // Raro: com o relógio múltiplo de 0x8000, tile do 3º tipo de terreno e
        // célula livre, uma AMBIENT_ANIM 0x87B (sub_405000(0x87B, 2, 2, 1, 0)).
        const auto& types = g_gameMap.terrainTypes();
        if ((TethysGame::tick() & 0x7FFF) == 0 && types.size() > 2 && types[2].word(0) <= tile &&
            types[2].word(2) >= tile && !(cellAt(tx0, ty0) & 0x7FF0000u)) {
            const int e = createAmbientAnim(toPX((tx0 & iMask()) << 5), ty0 << 5, 0x87B, 2, 2);
            if (e) g_units[e].flags |= 0x10000000u;
        }
    }
    killUnit(id, artLastFrameRaw(meteorDieAnim(sz)));
}

static void meteorTick(int id) {                             // sub_44A210
    if (!warningPhases(id, kFmtMeteorWarn, 0x91, kFmtMeteorAlert, 0x92)) return;
    UnitRecord& m = g_units[id];
    if (!(m.flags & 0x2000u)) {
        const int16_t sz = static_cast<int16_t>(m.cargo);
        if (sz < 0 || sz > 2) m.cargo = static_cast<uint16_t>(TethysGame::getRand(3));
        m.dsTX = static_cast<int16_t>(ix(m) >> 5);
        m.dsTY = static_cast<int16_t>(iy(m) >> 5);
        // (som 0x54 em (x,y).)
        postAt(m, kFmtMeteorHit, 0x93);
        // Começa 40 ticks antes, acima e à direita (vt[32] desce 15 px x 11 px por tick).
        m.pixelX = toPX((ix(m) + 0x258) & iPxMask());
        m.pixelY -= 0x1B8;
        m.flags |= 0x2000u;
        m.field22 = SUB_DRIFT;
        m.field3C = 5;
        m.dsTick = TethysGame::tick();
        disasterMorale(kMeteor);
        return;
    }
    if (--m.field3C == 2) meteorDefense(id);
    UnitRecord& q = g_units[id];
    if (q.field3C == 1) q.flags &= ~(0x40000u | 0x10000u);
    // (passos pares > 2: som 0x54.)
    if (q.field3C == 0) meteorImpact(id);
}

// =====================================================================================
// RAIO 0x4C (vt[20] sub_432F10, vt[32] sub_432E40, vt[6] sub_433680)
// =====================================================================================
static const int kBoltAnim[3] = {0x54A, 0x5B6, 0x5B7};      // dword_4E0BF8

static void offsets4(int raw, int frame, int8_t out[4]) {
    const int8_t* o = artFrameOffsets(raw, frame);
    for (int k = 0; k < 4; ++k) out[k] = o[k];
}

// Raio caindo: ponto pela nuvem (frame 0 de 0x542/0x545/0x548) + 1º frame do
// raio com y1 >= 10; dano por distância até o raio (desc+0x248) da arma 0x4C.
static void lightningStrike(int id) {
    const UnitRecord& L = g_units[id];
    const WeaponSheetDesc* wd = weaponSheet(kLightning);
    if (!wd) return;
    const WeapPlayerStats& w = wd->player[(L.owner >> 4) & 0x0F];
    const int bolt = kBoltAnim[(L.dsAux >> 4) % 3];
    const int radius = wd->radius;
    const int rT = (radius + 31) >> 5;
    int8_t o542[4], o548[4], o545[4], ob[4];
    offsets4(0x542, 0, o542);
    offsets4(0x548, 0, o548);
    offsets4(0x545, 0, o545);
    const int nb = artLastFrameRaw(bolt);
    for (int f = 0;; ++f) {
        offsets4(bolt, f, ob);
        if (ob[1] >= 10 || f >= nb) break;
    }
    int sx, sy;                                              // var_80 / var_A0
    if (L.dsAux & 1) { sx = o545[0] + o542[0] + ix(L); sy = o545[1] + o542[1] + iy(L); }
    else             { sx = o542[2] + o548[0] + ix(L); sy = o542[3] + o548[1] + iy(L); }
    const int tx = (sx + (ob[2] + ob[0]) / 2) >> 5;
    const int ty = (sy + ob[1]) >> 5;
    int y0 = ty - rT; if (y0 <= 0) y0 = 0;
    int y1 = ty + rT + 1; if (mapH() < y1) y1 = mapH();
    int reach = radius;                                      // ebp: prédios trocam p/ max(raio, 26)
    for (int x = tx - rT, cxp = x * 32 + 16; x <= tx + rT; ++x, cxp += 32) {
        for (int y = y0; y < y1; ++y) {
            const uint32_t cell = cellAt(x, y);
            if (!(cell & 0x7FF0000u)) {
                // Parede 0x17..0x19: sub_4A23F0(x, y, (raio − d) * pen / raio) — pendente.
                continue;
            }
            const int occ = damageableIn(cell);
            if (!occ) continue;
            const UnitRecord& t = g_units[occ];
            int ady, adx;
            if (t.flags & UNIT_BUILDING) {
                ady = std::abs(sy - y * 32 - 16);
                adx = std::abs(sx - cxp);
                reach = radius < 26 ? 26 : radius;
            } else {
                ady = std::abs(sy - iy(t));
                adx = std::abs(sx - ix(t));
            }
            if (iPxW() / 2 < adx) adx = iPxW() - adx;
            const int d = (adx < ady ? adx : ady) / 2 + (adx > ady ? adx : ady);
            if (reach <= d) continue;
            int base = ((armorOf(t) * w.concussion) >> 8) + w.penetration;
            if (TethysGame::cheatFlag(3)) base <<= 2;                    // dword_56DE64
            int dmg;
            if (!(t.flags & UNIT_BUILDING)) {
                dmg = (radius - d) * base / radius;
            } else {
                dmg = (10000 - colonyDirtProtection(t)) * base / 20000;
                reach = radius;
            }
            if (dmg > 0) addDamage(occ, dmg);
        }
    }
}

static void lightningTick(int id) {                          // sub_432F10
    if (!warningPhases(id, kFmtStormWarn, 0x8E, kFmtStormAlert, 0x8F)) return;
    UnitRecord& L = g_units[id];
    const int now = TethysGame::tick();
    if (!(L.flags & 0x2000u)) {
        L.flags |= 0x2000u;
        L.dsTX = static_cast<int16_t>(ix(L) >> 5);
        L.dsTick = now;
        L.dsTY = static_cast<int16_t>(iy(L) >> 5);
        L.dsAux = now + TethysGame::getRand(0x1E) + 0xC8;    // 1º raio
        postAt(L, kFmtStormHit, 0x90);
        disasterMorale(kLightning);
        if (L.dsEndX == -1)
            L.dsEndX = static_cast<int16_t>(xOff() ? xOff() + TethysGame::getRand(iW() >> 1)
                                                   : TethysGame::getRand(iW()));
        if (L.dsEndY == -1) L.dsEndY = static_cast<int16_t>(TethysGame::getRand(mapH()));
        const int dx = wrapTileD(L.dsEndX - L.dsTX);
        const int dy = TethysGame::getRand(10) - L.dsTY;
        const int r2 = TethysGame::getRand(10);
        L.dsDir = dirOfVec(dx + r2 - 5, dy + L.dsEndY - 5);
        L.field3C = static_cast<int16_t>(L.cargo) * 100 - 1;
        L.field22 = SUB_DRIFT;
        return;
    }
    const int since = now - L.dsAux;
    if (--L.field3C <= 0) { killUnit(id, 12); return; }
    if (since < 0) return;
    if (since >= 8) {
        if (since > 0x32) L.dsAux += TethysGame::getRand(0x1E) + 0x96;   // próximo raio
        return;
    }
    lightningStrike(id);
}

static void lightningMove(UnitRecord& L) {                   // sub_432E40
    const int16_t ax = static_cast<int16_t>(L.dsAccX + dirCos(L.dsDir) * 4);
    const int16_t ay = static_cast<int16_t>(L.dsAccY + dirSin(L.dsDir) * 4);
    int x = ix(L) + (ax >> 8);
    const int y = iy(L) + (ay >> 8);
    L.dsAccX = static_cast<int16_t>(ax & 0xFF);
    L.dsAccY = static_cast<int16_t>(ay & 0xFF);
    if (xOff()) {                                            // só mapas estreitos prendem x
        if (x <= 0) x = 0;
        if (iPxW() < x) x = iPxW();
    }
    int yc = y > 0 ? y : 0;
    if (mapH() * 32 < yc) yc = mapH() * 32;
    L.pixelX = toPX(x);
    L.pixelY = yc;
    if (yc != y && L.action != ACT_DIE) L.field3C = 1;       // bateu na borda: acaba
}

// =====================================================================================
// TORNADO 0x4D (vt[20] sub_48F430, vt[32] sub_490090, vt[6] sub_48FE10, vt[30] sub_48FBA0)
// =====================================================================================
static bool facesLeft(uint8_t dir) { return dir >= 0x40 && dir < 0xC0; }

static void tornadoDamage(int id) {
    const UnitRecord& T = g_units[id];
    const int now = TethysGame::tick();
    int anim, frame;
    if (T.dsAux) {
        anim = facesLeft(T.dsDir) ? 0x4C0 : 0x4C1;
        const int lim = artLastFrameRaw(anim) - 1;
        const int e = now - T.dsAux - 1;
        frame = e > lim ? e : lim;                           // (sic: o desenho usa o menor)
    } else {
        anim = facesLeft(T.dsDir) ? 0x4C2 : 0x4C3;
        frame = now % (artLastFrameRaw(anim) + 1);
    }
    const WeaponSheetDesc* wd = weaponSheet(kTornado);
    if (!wd) return;
    const int rT = (wd->radius + 31) >> 5;
    const int conc = wd->player[0].concussion;               // off_4E1474[+0x30]
    int8_t o[4];
    offsets4(anim, frame, o);
    const int cy = (iy(T) + o[1]) >> 5;
    const int cx = ((ix(T) + o[0]) >> 5) & iMask();
    for (int y = cy - rT; y <= cy + rT; ++y) {
        if (y < 0 || y >= mapH()) continue;
        for (int x = cx - rT; x <= cx + rT; ++x) {
            const uint32_t cell = cellAt(x, y);
            if (!(cell & 0x7FF0000u)) {
                // Parede 0x17..0x19: sub_4A23F0(x, y, conc, ou conc/2 fora da
                // coluna central) — pendente.
                continue;
            }
            const int occ = damageableIn(cell);
            if (!occ) continue;
            const int adx = std::abs(x - cx), ady = std::abs(y - cy);
            const int d = ady < adx ? adx + ady / 2 : ady + adx / 2;
            if (d >= rT) continue;
            int dmg = (rT - d) * conc / rT;
            if (g_units[occ].flags & UNIT_BUILDING)
                dmg = (10000 - colonyDirtProtection(g_units[occ])) * dmg / 10000;
            addDamage(occ, dmg);
        }
    }
}

// Rumo: perto da reta partida→destino, 1/20 de chance de virar p/ o destino
// ±64; longe dela, mira o destino (±5 tiles). Trocar de lado (esquerda/direita)
// inicia a animação da curva ([+0x58] = tick).
static void tornadoSteer(int id) {
    UnitRecord& T = g_units[id];
    const int now = TethysGame::tick();
    const int sdx = wrapTileD(T.dsEndX - T.dsTX);
    int a, b;
    if (sdx != 0) {
        a = (ix(T) >> 5) * (T.dsEndY - T.dsTY) / sdx;
        b = T.dsTY + (T.dsTY - T.dsEndY) * T.dsTX / sdx;
    } else {
        a = 10000; b = 0;
    }
    const int cyT = iy(T) >> 5;
    if (a + b - 3 <= cyT && a + b + 3 >= cyT) {
        if (TethysGame::getRand(20) == 0) {
            const bool side = facesLeft(T.dsDir);
            const uint8_t toEnd = dirOfVec(wrapTileD(T.dsEndX - (ix(T) >> 5)), T.dsEndY - cyT);
            T.dsDir = static_cast<uint8_t>(toEnd + TethysGame::getRand(0x80) - 0x40);
            if (facesLeft(T.dsDir) != side) T.dsAux = now;
        }
    } else {
        const bool side = facesLeft(T.dsDir);
        const int ex = wrapTileD(T.dsEndX - (ix(T) >> 5));
        const int ey = T.dsEndY + TethysGame::getRand(10) - cyT;
        const int r2 = TethysGame::getRand(10);
        T.dsDir = dirOfVec(ex + r2 - 5, ey - 5);
        if (facesLeft(T.dsDir) != side) T.dsAux = now;
    }
    if (T.dsAux && artLastFrameRaw(0x4C1) - 1 <= now - T.dsAux) T.dsAux = 0;
}

static void tornadoTick(int id) {                            // sub_48F430
    if (!warningPhases(id, kFmtVortexWarn, 0x8B, kFmtVortexAlert, 0x8C)) return;
    UnitRecord& T = g_units[id];
    const int now = TethysGame::tick();
    if (!(T.flags & 0x2000u)) {
        T.dsAux = 0;
        T.dsTick = now;
        T.dsTX = static_cast<int16_t>(ix(T) >> 5);
        T.dsTY = static_cast<int16_t>(iy(T) >> 5);
        if (T.dsEndX == -1)
            T.dsEndX = static_cast<int16_t>(xOff() ? xOff() + TethysGame::getRand(iW() >> 1)
                                                   : TethysGame::getRand(iW()));
        if (T.dsEndY == -1) T.dsEndY = static_cast<int16_t>(TethysGame::getRand(mapH()));
        const int dx = wrapTileD(T.dsTX - T.dsEndX);         // (sic: partida − destino)
        const int dy = TethysGame::getRand(10) - T.dsTY;
        const int r2 = TethysGame::getRand(10);
        T.dsDir = dirOfVec(dx + r2 - 5, dy + T.dsEndY - 5);
        T.field22 = SUB_DRIFT;
        T.flags |= 0x2000u;
        T.field3C = static_cast<int16_t>(T.cargo) * 8 - 1;
        // (som 0x51.)
        postAt(T, kFmtVortexHit, 0x8D);
        disasterMorale(kTornado);
        return;
    }
    if (--T.field3C == 0) { killUnit(id, 30); return; }
    // (a cada 3 passos, som 0x51.)
    if (now - T.dsTick > artLastFrameRaw(0x4BE) + 30) tornadoDamage(id);   // já pousou
    tornadoSteer(id);
}

static void tornadoMove(UnitRecord& T) {                     // sub_490090
    const int16_t ax = static_cast<int16_t>(T.dsAccX + dirCos(T.dsDir) * 2);
    const int16_t ay = static_cast<int16_t>(T.dsAccY + dirSin(T.dsDir) * 2);
    int x = ix(T) + (ax >> 8);
    int y = iy(T) + (ay >> 8);
    T.dsAccX = static_cast<int16_t>(ax & 0xFF);
    T.dsAccY = static_cast<int16_t>(ay & 0xFF);
    if (x <= 0) x = 0;
    if (x >= iPxW()) x = iPxW();
    if (y <= 0) y = 0;
    if (y >= mapH() * 32) y = mapH() * 32;
    T.pixelX = toPX(x);
    T.pixelY = y;
}

// =====================================================================================
// ERUPÇÃO 0x4F (vt[20] sub_4A81A0) e LAVA (dword_54E3A0, classe off_4D0358)
// =====================================================================================
// Espalhador (base off_4D6A80, compartilhada com o micróbio dword_57AB98): a
// cada turno (sub_489DD0) sorteia [+0x10] células por um LFSR ([+4]) e chama
// vt[0] em cada uma. Bits da célula: 0x10000000 lava possível, 0x8000000 lava.
struct Spreader { uint32_t lfsr = 0x515; int toggle = 0; int count = 0; };
static Spreader s_lava;

void lavaReset() { s_lava.count = 0; s_lava.lfsr = 0x515; s_lava.toggle = 0; }   // sub_4322A0

void setLavaSpeed(int speed) {                               // sub_49E240 (?SetLavaSpeed)
    s_lava.lfsr = 0x710071;
    s_lava.toggle = 0;
    s_lava.count = iW() * mapH() * speed / 10000;            // dword_54EBE4 = área interna
}

void setLavaPossible(int x, int y, bool on) {                // ?SetLavaPossible (x do port)
    if (x < 0 || x >= g_gameMap.width() || y < 0 || y >= mapH()) return;
    uint32_t c = g_gameMap.rawCell(x, y) & ~0x10000000u;
    if (on) c |= 0x10000000u;
    g_gameMap.setRawCell(x, y, c);
}

// Tipos de terreno: sub_48B930 tile-base da lava do tipo que contém o tile
// (+0xB4; sem tipo: 0x15); sub_48B980/48B9C0/48BA00: +0xB6/+0xB8/+0xBA do 1º
// tipo com lava (+0xB4 > 0; nenhum: 1).
static int lavaBaseTile(int tile) {
    for (const auto& t : g_gameMap.terrainTypes())
        if (t.word(0) <= tile && t.word(2) >= tile) return t.word(0xB4);
    return 0x15;
}
static int lavaField(int off) {
    for (const auto& t : g_gameMap.terrainTypes())
        if (t.word(0xB4) > 0) return t.word(off);
    return 1;
}

// Vizinhos com lava: 1 cima, 2 esquerda, 4 baixo, 8 direita (sem cima na
// linha 0 e sem baixo na última).
static int lavaMask(int x, int y) {
    auto has = [](int tx, int ty) { return (cellAt(tx & iMask(), ty) & 0x8000000u) != 0; };
    int m = (has(x - 1, y) ? 2 : 0) | (has(x + 1, y) ? 8 : 0);
    if (y != 0) m |= has(x, y - 1) ? 1 : 0;
    if (mapH() - y != 1) m |= has(x, y + 1) ? 4 : 0;
    return m;
}
// Tile da lava pela máscara; cercada (0xF): uma das variantes ao acaso.
static int lavaTileFor(int x, int y, int mask) {
    const int base = lavaBaseTile(static_cast<int>((cellAt(x, y) & 0xFFE0u) >> 5));
    if (mask == 0xF) {
        const int edge = lavaField(0xB6) * 15;
        const int r = TethysGame::getRand(lavaField(0xB8) - 1);
        return r * lavaField(0xBA) + base + edge;
    }
    return lavaField(0xB6) * mask + base;
}

static void lavaRetile(int cx, int cy) {                     // sub_432560 (vt[4])
    int y0 = cy - 1; if (y0 <= 0) y0 = 0;
    int y1 = cy + 1; if (y1 >= mapH() - 1) y1 = mapH() - 1;
    for (int x = cx - 1; x <= cx + 1; ++x)
        for (int y = y0; y <= y1; ++y) {
            const int tx = x & iMask();
            if (!(cellAt(tx, y) & 0x8000000u)) continue;
            setTileAt(tx, y, lavaTileFor(tx, y, lavaMask(tx, y)));
        }
}

static void lavaSetCell(int x, int y) {                      // sub_432890 (vt[2])
    x &= iMask();
    if (y < 0 || mapH() - 1 < y) return;
    const uint32_t c = cellAt(x, y);
    if (!(c & 0x10000000u)) return;
    if ((c & 0x1F) == 0x19) return;   // parede de lava: sub_4A23F0(x, y, 15) — dano em parede pendente
    const int occ = static_cast<int>((c & 0x7FF0000u) >> 16);
    if (occ && occ < static_cast<int>(g_units.size())) {
        UnitRecord& t = g_units[occ];
        if ((t.flags & UNIT_VEHICLE) && t.id != -1 && !(t.flags & UNIT_DEAD) && (t.flags & UNIT_DAMAGEABLE)) {
            t.damage = static_cast<int16_t>(maxHPOf(t));
            killUnit(occ, 1);
            t.flags |= UNIT_DEAD;
        }
    }
    int px;
    portTile(x, y, px);
    if ((cellAt(x, y) & 0x1F) == 0x1A) {
        // Prédio na célula (sub_437EB0/437EE0, filtro 4): morre; senão o tubo some.
        const int b = static_cast<int>((cellAt(x, y) & 0x7FF0000u) >> 16);
        if (b && b < static_cast<int>(g_units.size()) && (g_units[b].flags & UNIT_BUILDING)) {
            UnitRecord& t = g_units[b];
            if (t.id == -1 || (t.flags & UNIT_DEAD)) return;
            t.damage = static_cast<int16_t>(maxHPOf(t));
            killUnit(b, 1);
            g_units[b].flags |= UNIT_DEAD;
            return;
        }
        destroyWallOrTube(px, y);
        return;
    }
    destroyWallOrTube(px, y);
    // Magma vent (0x52) / fumarola (0x53) no tile (lista dword_5737D0): Kill(1).
    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& v = g_units[i];
        if (v.id == -1 || (v.type != 0x52 && v.type != 0x53)) continue;
        if ((unitOY(v) >> 5) != y || (toIX(unitOX(v)) >> 5) != x) continue;
        killUnit(static_cast<int>(i), 1);
        break;
    }
    setTileAt(x, y, lavaTileFor(x, y, lavaMask(x, y)));
    setCellAt(x, y, cellAt(x, y) | 0x8000000u);
    // sub_49E680 do micróbio: a lava limpa 0x40000000 em (x,y)-(x+1,y+1) (o
    // reajuste do micróbio, vt[4], entra com o micróbio).
    setCellAt(x, y, cellAt(x, y) & ~0x40000000u);
    setCellAt((x + 1) & iMask(), y, cellAt((x + 1) & iMask(), y) & ~0x40000000u);
    if (mapH() - 1 > y) {
        setCellAt(x, y + 1, cellAt(x, y + 1) & ~0x40000000u);
        setCellAt((x + 1) & iMask(), y + 1, cellAt((x + 1) & iMask(), y + 1) & ~0x40000000u);
    }
    lavaRetile(x, y);
}

// sub_4322C0 (vt[0]): célula com lava e algum vizinho sem → tenta uma direção ao acaso.
static void lavaSpreadAt(int x, int y) {
    static const int kDir[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};   // unk_4E0BD8
    x &= iMask();
    if (!(cellAt(x, y) & 0x8000000u)) return;
    if (lavaMask(x, y) == 0xF) return;
    const int r = TethysGame::getRand(4);
    lavaSetCell(x + kDir[r][0], y + kDir[r][1]);
}

void lavaTick() {                                            // sub_49DC20 (lava)
    if (!g_gameMap.ok() || s_lava.count <= 0) return;
    int lgW = 0;
    while ((1 << lgW) < iW()) ++lgW;                         // dword_54EBCC
    uint32_t v = s_lava.lfsr;
    for (int i = 0; i < s_lava.count; ++i) {
        const bool fb = (((v >> 13) ^ (v >> 16)) & 1) != 0;  // paridade de 0x12000
        v <<= 1;
        if (fb) v += 1;
        const int x = static_cast<int>(v) & iMask();
        const int y = (static_cast<int>(v) >> lgW) & (mapH() - 1);
        lavaSpreadAt(x, y);
    }
    s_lava.lfsr = v;
}

static void eruptionTick(int id) {                           // sub_4A81A0
    if (!warningPhases(id, kFmtLavaWarn, 0x88, kFmtLavaAlert, 0x89)) return;
    const UnitRecord& e = g_units[id];
    lavaSetCell(ix(e) >> 5, iy(e) >> 5);
    setLavaSpeed(static_cast<int16_t>(g_units[id].cargo));
    postAt(g_units[id], kFmtLavaHit, 0x8A);
    g_units[id].field22 = SUB_REMOVE;
}

// =====================================================================================
bool disasterPeriodicTick(int id) {
    switch (g_units[id].type) {
        case kLightning:  lightningTick(id); return true;
        case kTornado:    tornadoTick(id); return true;
        case kEarthquake: earthquakeTick(id); return true;
        case kEruption:   eruptionTick(id); return true;
        case kMeteor:     meteorTick(id); return true;
        default: return false;
    }
}

bool disasterMoveTick(int id) {                              // case 16 → vt[32]
    UnitRecord& r = g_units[id];
    switch (r.type) {
        case kMeteor:                                        // sub_44ACB0
            r.pixelX = toPX((ix(r) - 15) & iPxMask());
            r.pixelY += 11;
            return true;
        case kLightning: lightningMove(r); return true;
        case kTornado:   tornadoMove(r); return true;
        default: return false;
    }
}

void disasterShake(int viewCx, int viewCy, int frameSkip, int& shakeX, int& shakeY) {   // sub_413560
    const int W = g_gameMap.width() * 32;
    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& q = g_units[i];
        if (q.id == -1 || q.type != kEarthquake || !(q.flags & 0x2000u)) continue;
        int cx = viewCx;
        if (cx > W / 2) cx = W - cx;
        const int dy = std::abs(unitOY(q) - viewCy);
        int dx = std::abs(unitOX(q) - cx);
        if (W / 2 < dx) dx = std::abs(W - dx);
        const int mag = static_cast<int16_t>(q.cargo);
        int d = 2 * (dx > dy ? dx : dy) + (dx < dy ? dx : dy);
        d = (d & ~0x3F) << 2;
        const int intensity = (mag * 0x600 - d) / (mag * 6);
        const int phase = (q.flags & 0x10000u) ? ((mag * 8 - q.field3C) << 8) / (mag * 8)
                                               : (q.field3C << 8) / (mag * 8);
        const int s = q.quakeAmp * (phase * intensity);
        const int ex = static_cast<int>((static_cast<uint32_t>(s * q.quakeDir)) & 0xFEFFFFFFu) >> 24;
        const int ey = ((0x100 - std::abs(q.quakeDir)) * s) >> 24;
        if (TethysGame::tick() & frameSkip) { shakeX += ex; shakeY += ey; }
        else { shakeX -= ex; shakeY -= ey; }
    }
}

// ---- Desenho ----------------------------------------------------------------------
static void sprite(std::vector<WeaponDrawOp>& ops, int raw, int frame, int x, int y) {
    if (raw <= 0) return;                                    // (anim 0: nada a desenhar)
    WeaponDrawOp op;
    op.kind = WeaponDrawOp::SPRITE;
    op.anim = raw - kAnimBias;
    op.frame = frame;
    op.x = toPX(x & iPxMask());
    op.y = y;
    ops.push_back(op);
}

static void meteorDraw(const UnitRecord& m, std::vector<WeaponDrawOp>& ops) {     // sub_44A9A0
    static const int kIntro[3] = {0x46B, 0x469, 0x46A}, kLoop[3] = {0x468, 0x452, 0x453};
    const int sz = static_cast<int16_t>(m.cargo);
    if (sz < 0 || sz > 2) return;
    const int now = TethysGame::tick();
    int anim, frame;
    if (m.action == ACT_DIE) {
        anim = meteorDieAnim(sz);
        frame = artLastFrameRaw(anim) - m.field3C;
    } else {
        const int e = now - m.dsTick;
        if (artLastFrameRaw(kIntro[sz]) >= e) { anim = kIntro[sz]; frame = e; }
        else { anim = kLoop[sz]; frame = e % (artLastFrameRaw(kLoop[sz]) + 1); }
    }
    sprite(ops, anim, frame, ix(m), iy(m));
    if ((m.flags & 0x40000u) && m.dsAux > 0 && m.dsAux < static_cast<int>(g_units.size())) {
        // Laser da defesa (sub_41E8F0, cor 0x7C0000): mira 32 px adiante se errou.
        // (sons 0x47 na defesa e 0x4E no meteoro destruído.)
        const int lead = (m.flags & 0x10000u) ? 0 : 32;
        int dx, dy;
        origPixel(g_units[m.dsAux], dx, dy);
        WeaponDrawOp op;
        op.kind = WeaponDrawOp::LASER_LINE;
        op.x = dx; op.y = dy;
        op.x2 = toPX((ix(m) + lead) & iPxMask()); op.y2 = iy(m) - lead;
        ops.push_back(op);
    }
}

static void lightningDraw(const UnitRecord& L, std::vector<WeaponDrawOp>& ops) {  // sub_433680
    const int now = TethysGame::tick();
    // Balanço das nuvens laterais pelo relógio.
    const int i1 = ((now & 0x7F) << 4) >> 3, i2 = ((now * 11) & 0x7F8) >> 3;
    const int w10 = (dirCos(static_cast<uint8_t>(i1)) / 4 - dirCos(static_cast<uint8_t>(i2)) / 7) / 6;
    const int w0C = (dirSin(static_cast<uint8_t>(i1)) / 4 - dirSin(static_cast<uint8_t>(i2)) / 7) / 6;
    const int j1 = (((now - 0x38) & 0x7F) << 4) >> 3;
    const int j2 = ((now + 0x48 + (now * 5 + 0x168) * 2) & 0x7F8) >> 3;
    const int w8 = (dirCos(static_cast<uint8_t>(j1)) / 4 - dirCos(static_cast<uint8_t>(j2)) / 7) / 6;
    const int w4 = (dirSin(static_cast<uint8_t>(j1)) / 4 - dirSin(static_cast<uint8_t>(j2)) / 7) / 6;
    int esi = now - L.dsTick;
    const int sinceStrike = now - L.dsAux;
    int a1, a2, a3;
    if (L.action == ACT_DIE) {                               // dissipando (Kill(12))
        esi = 12 - L.field3C;
        a1 = artLastFrameRaw(0x543) * 3 < esi ? 0 : 0x543;
        a2 = artLastFrameRaw(0x546) * 2 < esi ? 0 : 0x546;
        a3 = artLastFrameRaw(0x549) * 2 < esi ? 0 : 0x549;
    } else {                                                 // formando → ativa
        a1 = 0x541 + (artLastFrameRaw(0x541) * 3 <= esi ? 1 : 0);
        a2 = 0x544 + (artLastFrameRaw(0x544) * 2 <= esi ? 1 : 0);
        a3 = 0x547 + (artLastFrameRaw(0x547) * 2 <= esi ? 1 : 0);
    }
    const int f1 = (esi / 3) % (artLastFrameRaw(a1) + 1);
    int8_t o1[4], o2[4] = {0, 0, 0, 0}, o3[4] = {0, 0, 0, 0}, ob[4];
    offsets4(a1, f1, o1);
    const int f2 = (esi / 2) % (artLastFrameRaw(a2) + 1);
    const int f3 = (esi / 2) % (artLastFrameRaw(a3) + 1);
    if (a3) offsets4(a3, f3, o3);
    if (a2) offsets4(a2, f2, o2);
    if (sinceStrike >= 0) {
        // (sinceStrike == 0: som 0x52.)
        const int bolt = kBoltAnim[(L.dsAux >> 4) % 3];
        if (artLastFrameRaw(a2) * 4 > sinceStrike) {
            const int fb = sinceStrike % (artLastFrameRaw(bolt) + 1);
            offsets4(bolt, fb, ob);
            if (ob[1] > 10) {
                // (o original também marca 0x8000000 — clarão — no raio e nas
                // unidades a rT tiles do ponto atingido.)
                int X, Y;
                if (L.dsAux & 1) { X = o2[0] + o1[0] + ix(L); Y = o2[1] + o1[1] + iy(L); }
                else             { X = o3[0] + o1[2] + ix(L); Y = o3[1] + o1[3] + iy(L); }
                sprite(ops, bolt, fb, X, Y);
            }
        }
    }
    sprite(ops, a1, f1, ix(L), iy(L));
    if (a2) sprite(ops, a2, f2, ix(L) + o1[0] + w10, iy(L) + o1[1] + w0C);
    if (a3) sprite(ops, a3, f3, ix(L) + o1[2] - w8, iy(L) + o1[3] + w4);
}

static void tornadoFunnel(const UnitRecord& T, int now, int& anim, int& frame) {
    if (T.dsAux) {
        anim = facesLeft(T.dsDir) ? 0x4C0 : 0x4C1;
        const int e = now - T.dsAux - 1;
        const int lim = artLastFrameRaw(anim) - 1;
        frame = lim >= e ? e : lim;
    } else {
        anim = facesLeft(T.dsDir) ? 0x4C2 : 0x4C3;
        frame = now % (artLastFrameRaw(anim) + 1);
    }
}

static void tornadoShadowDraw(const UnitRecord& T, std::vector<WeaponDrawOp>& ops) {   // sub_48FBA0
    const int now = TethysGame::tick();
    int mode = 1, v8 = 0;                                    // 0 pousando, 1 poeira, 2 sumindo, 3 nada
    if (T.action == ACT_DIE && T.field3C <= 30) {
        v8 = 30 - T.field3C;
        mode = artLastFrameRaw(0x4C6) > v8 ? 2 : 3;
    }
    const int nI = artLastFrameRaw(0x4BE), e = now - T.dsTick;
    if (e <= nI) mode = 3;
    else if (nI + 30 >= e) mode = 3;
    else if (nI + artLastFrameRaw(0x4C4) + 30 >= e) { mode = 0; v8 = e - nI - 31; }
    int anim, frame;
    tornadoFunnel(T, now, anim, frame);
    int8_t o[4];
    offsets4(anim, frame, o);
    const int X = ix(T) + o[0], Y = iy(T) + o[1];
    if (mode == 2) sprite(ops, 0x4C6, v8, X, Y);
    else if (mode == 1) sprite(ops, 0x4C5, now % (artLastFrameRaw(0x4C5) + 1), X, Y);
    else if (mode == 0) sprite(ops, 0x4C4, v8, X, Y);
}

static void tornadoDraw(const UnitRecord& T, std::vector<WeaponDrawOp>& ops) {     // sub_48FE10
    const int now = TethysGame::tick();
    int v8 = 0, base = 1, funnel = 1, v4 = 0;               // base: 0 pouso, 1 redemoinho, 2 colapso
    if (T.action == ACT_DIE) {
        v4 = artLastFrameRaw(0x4BF) - T.field3C;
        if (v4 >= 0) { base = 2; funnel = 3; }
        if (T.field3C <= 30) v8 = 30 - T.field3C;
    }
    const int nI = artLastFrameRaw(0x4BE), e = now - T.dsTick;
    if (nI >= e) { base = 0; funnel = 3; }
    else if (nI + 30 >= e) { funnel = 0; v8 = nI + 30 - e; }
    int anim, frame;
    tornadoFunnel(T, now, anim, frame);
    if (funnel != 3) sprite(ops, anim, frame, ix(T), iy(T) - v8 * 2);
    if (base == 2) sprite(ops, 0x4BF, v4, ix(T), iy(T));
    else if (base == 1) sprite(ops, 0x451, (now >> 1) % (artLastFrameRaw(0x451) + 1), ix(T), iy(T));
    else if (base == 0) sprite(ops, 0x4BE, now - T.dsTick - 1, ix(T), iy(T));
}

void disasterDrawOps(int id, bool shadowPass, std::vector<WeaponDrawOp>& ops) {
    const UnitRecord& r = g_units[id];
    if (!(r.flags & 0x2000u)) return;                        // vt[9]: só depois de começar
    if (shadowPass) {                                        // vt[30] (flag 0x10000000)
        if (r.type == kTornado && (r.flags & 0x10000000u)) tornadoShadowDraw(r, ops);
        return;
    }
    switch (r.type) {
        case kMeteor:    meteorDraw(r, ops); break;
        case kLightning: lightningDraw(r, ops); break;
        case kTornado:   tornadoDraw(r, ops); break;
        default: break;
    }
}

} // namespace op2
