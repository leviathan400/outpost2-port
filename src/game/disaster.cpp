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

namespace op2 {

static const int kEarthquake = 0x4E;

// Format[0x201..0x20F] do EXE: Warning / ALERT / DANGER por desastre.
static const char* const kFmtQuakeWarn  = "Warning: Quake expected at %d,%d";   // 0x207
static const char* const kFmtQuakeAlert = "ALERT: Quake expected at %d,%d";     // 0x208
static const char* const kFmtQuakeHit   = "DANGER: Quake detected at %d,%d";    // 0x209

void setEarthquake(int x, int y, int magnitude) {           // ?SetEarthquake
    if (magnitude == 0) magnitude = 1;
    Unit u;
    createUnit(u, kEarthquake, LOCATION{x, y}, 6, magnitude);   // sub_4466B0(0x4E, .., 6, mag, 0, 1)
}

// Satélite EDWARD (0x58) do jogador (sub_490800).
static bool hasEdward(int player) {
    for (size_t i = 1; i < g_units.size(); ++i)
        if (g_units[i].id != -1 && g_units[i].type == 0x58 && (g_units[i].owner & 0x0F) == player) return true;
    return false;
}

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
        for (int p = 0; p < 6; ++p)                          // sub_44F8E0: moral
            moraleDisaster(p, researchHas(p, researchTopicIndex(kEarthquake)));
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

bool disasterPeriodicTick(int id) {
    switch (g_units[id].type) {
        case kEarthquake: earthquakeTick(id); return true;
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

} // namespace op2
