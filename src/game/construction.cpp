#include "construction.h"
#include "../audio/sound.h"

#include "building_sheet.h"
#include "command.h"
#include "docking.h"
#include "game_map.h"
#include "messages.h"
#include "pathfinder.h"
#include "player.h"
#include "scenario.h"
#include "structure.h"
#include "terrain_cost.h"
#include "tethys_game.h"
#include "unit.h"
#include "ai_group.h"

namespace op2 {

static bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
static int wmask() { return g_gameMap.width() - 1; }
// vt[0x74] (aceita comando) — a mesma aproximação de commands.cpp.
static bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}

bool kitFootprint(int convecId, int& w, int& h) {
    if (!valid(convecId)) return false;
    const StructSheetDesc* d = structSheet(static_cast<int16_t>(g_units[convecId].cargo));
    if (!d) return false;
    w = d->sizeX; h = d->sizeY;                          // desc+0x248 / +0x24A
    return true;
}

void kitPlacementRect(int w, int h, int tileX, int tileY, int& l, int& t, int& r, int& b) {   // sub_41B960
    // Cursor no centro do tile; meia área em pixels (+16 se o nº de tiles é ímpar).
    const int mx = tileX * 32 + 16, my = tileY * 32 + 16;
    const int w32 = w * 32, h32 = h * 32;
    int left = mx - w32 / 2 + ((w32 & 0x20) ? 0x10 : 0);
    int top = my - h32 / 2 + ((h32 & 0x20) ? 0x10 : 0);
    left >>= 5; top >>= 5;
    r = (left + w) & wmask();
    b = top + h;
    l = (left - 1) & wmask();
    t = top - 1;
}

// sub_40E210 case 6 → vt[0x84] da ConVec (sub_423310).
static void setBuildOrder(UnitRecord& u, uint32_t loc, int l, int t, int r, int b) {
    u.action = 6;
    u.field20 = 1;
    u.field3C = 0;
    u.hasOrder = true;
    u.order = OrderBlock{};
    u.order.count = 1;
    u.order.countMinus1 = 0;
    u.order.waypoints.assign(1, loc);
    u.order.flags |= 1;
    u.routeDirs.clear(); u.routeIdx = 0;
    u.buildL = static_cast<int16_t>(l); u.buildT = static_cast<int16_t>(t);   // [+0x62]/[+0x64]
    u.buildR = static_cast<int16_t>(r); u.buildB = static_cast<int16_t>(b);   // [+0x66]/[+0x68]
}

void mineFootprint(int& w, int& h) {
    const StructSheetDesc* d = structSheet(0x15);
    w = d ? d->sizeX : 1; h = d ? d->sizeY : 1;
}

// sub_40E210 case 6: unidade comandável; ConVec só com kit; [+0x23] != 2 →
// vt[0x84] (sub_423310 / sub_4233A0 / sub_423430: idênticos).
static void sendBuildPacket(const std::vector<int>& sel, int w, int h, int tileX, int tileY) {
    const int me = TethysGame::localPlayer();
    int l, t, r, b;
    kitPlacementRect(w, h, tileX, tileY, l, t, r, b);
    const uint32_t loc = packLocation(r, b);             // canto inferior direito do anel
    soundPlay(0x56);
    for (int id : sel) {
        if (!valid(id)) continue;
        UnitRecord& u = g_units[id];
        if (ownerOf(u) != me || !commandable(u)) continue;
        if (u.type == mapConVec && u.cargo == 0) continue;
        if (u.field23 == 2) continue;
        if (u.type != mapConVec && u.type != mapRoboMiner && u.type != mapGeoCon) continue;   // vt[0x84] nulo
        setBuildOrder(u, loc, l, t, r, b);
        u.buildGroup = -1;
    }
}

bool processBuildPacket(int owner, int id, unsigned int loc, int l, int t, int r, int b, int group) {
    if (!valid(id)) return false;
    UnitRecord& u = g_units[id];
    if (ownerOf(u) != (owner & 0x0F) || !commandable(u)) return false;
    if (u.type == mapConVec && u.cargo == 0) return false;
    if (u.field23 == 2) return false;
    if (u.type != mapConVec && u.type != mapRoboMiner && u.type != mapGeoCon) return false;   // vt[0x84] nulo
    setBuildOrder(u, loc, l, t, r, b);
    u.buildGroup = static_cast<int16_t>(group);          // [+0x6A]
    return true;
}

void sendBuildMine(const std::vector<int>& sel, int tileX, int tileY) {   // sub_452010
    int w, h;
    mineFootprint(w, h);
    sendBuildPacket(sel, w, h, tileX, tileY);
}

void sendBuildKit(const std::vector<int>& sel, int tileX, int tileY) {   // sub_451B30
    const int me = TethysGame::localPlayer();
    int w = 0, h = 0;
    for (int id : sel) if (kitFootprint(id, w, h)) break;
    if (!w) return;
    int l, t, r, b;
    kitPlacementRect(w, h, tileX, tileY, l, t, r, b);
    const uint32_t loc = packLocation(r, b);             // canto inferior direito do anel
    soundPlay(0x56);
    for (int id : sel) {                                 // case 6
        if (!valid(id)) continue;
        UnitRecord& u = g_units[id];
        if (ownerOf(u) != me || !commandable(u)) continue;
        if (u.type == mapConVec && u.cargo == 0) continue;
        if (u.field23 == 2) continue;
        if (u.type != mapConVec) continue;               // (Robo-Miner/GeoCon usam outra ordem)
        setBuildOrder(u, loc, l, t, r, b);
        u.buildGroup = -1;
    }
}

// sub_438A70: (x,y) é a doca de algum prédio?
static bool isDockOfBuilding(int x, int y) {
    const int occ = y > 0 ? g_gameMap.cellOccupant(x, y - 1) : 0;
    if (!valid(occ) || !(g_units[occ].flags & UNIT_BUILDING)) return false;
    int dx, dy;
    return dockCell(occ, dx, dy) && dx == x && dy == y;
}

// sub_4389E0 (interior) e sub_438B80 (anel): a célula aceita a obra? Contexto
// do pathfinder = a ConVec (dword_54F030[+0x10]).
static bool cellOkInterior(int self, int x, int y) {
    if (y < 0 || y >= g_gameMap.height()) return false;
    const uint32_t c = g_gameMap.rawCell(x, y);
    if (c & 0xAFFF0000u) {
        if (static_cast<int>((c & 0x7FF0000u) >> 16) != self) return false;
        if (c & 0xA8000000u) return false;
    }
    if (isDockOfBuilding(x, y)) return false;
    return pfCanEnter(x, y) != 0;                         // sub_4382C0
}
bool buildCellOk(int self, int x, int y) { return cellOkInterior(self, x, y); }
static bool cellOkRing(int self, int moveClass, int x, int y) {
    if (y < 0 || y >= g_gameMap.height()) return false;
    const uint32_t c = g_gameMap.rawCell(x, y);
    if (c & 0xAFFF0000u) {
        const int occ = static_cast<int>((c & 0x7FF0000u) >> 16);
        if (occ == self) { if (c & 0xA8000000u) return false; }
        else if (occ && valid(occ) && (g_units[occ].flags & UNIT_BUILDING)) return false;
    }
    if (c & 0xA8000000u) return false;
    if (!terrainMoveCost(static_cast<int>(c & 0x1F), moveClass)) return false;   // dword_4DEB9C
    return !isDockOfBuilding(x, y);
}

// sub_44C410: gira [+0x1C] rumo ao alvo (no máximo Turn Rate por tick); 1 = virado.
static bool turnTo(UnitRecord& u, uint8_t target) {
    const int8_t delta = static_cast<int8_t>(target - u.rotation);
    if (!delta) return true;
    const VehPlayerStats* vs = vehicleSheet(u.type, (u.owner >> 4) & 0x0F);
    int step = vs ? vs->turnRate : 8;
    if (TethysGame::cheatFlag(4)) step = 0x20;           // dword_56DE68
    const int ad = delta < 0 ? -delta : delta;
    if (step > ad) step = ad;
    u.rotation = static_cast<uint8_t>(u.rotation + (delta > 0 ? step : -step));
    u.curDir = static_cast<uint8_t>(((u.rotation + 16) & 0xFF) >> 5);
    return u.rotation == target;
}
bool unitTurnTo(UnitRecord& u, unsigned char target) { return turnTo(u, target); }

void conVecBuildTick(int id) {                            // jpt_43B312 case 6
    UnitRecord& u = g_units[id];
    if (u.field23 == 1 && (u.type == mapRoboMiner || u.type == mapGeoCon)) {   // loc_43B72F
        // Giro: Robo-Miner 0x60 (Eden) / 0x80 (Plymouth); GeoCon 0x40.
        const bool eden = ownerOf(u) < kMaxPlayers && g_playerData[ownerOf(u)].isEden;
        const uint8_t dir = u.type == mapRoboMiner ? (eden ? 0x60 : 0x80) : 0x40;
        if (!turnTo(u, dir)) return;
        int w, h;
        mineFootprint(w, h);                              // off_4E1394 (mina)
        pfSetContext(id);
        int cost = 0;
        bool ok = true;
        const int mc = u.moveClass;
        for (int i = 0; i < w + 2; ++i)
            for (int j = 0; j < h + 2; ++j) {
                const int x = (u.buildL + i) & wmask(), y = u.buildT + j;
                if (y >= 0 && y < g_gameMap.height()) {
                    if (TethysGame::cheatFlag(4)) ++cost;
                    else {
                        const int ct = static_cast<int>(g_gameMap.rawCell(x, y) & 0x1F);
                        if (ct != 0x15 && ct != 0x1A) cost += terrainMoveCost(ct, 3) >> 6;
                    }
                }
                const bool ring = i == 0 || j == 0 || i == w + 1 || j == h + 1;
                if (ring) { ok = ok && cellOkRing(id, mc, x, y); continue; }
                if (cellOkInterior(id, x, y)) continue;
                // Célula interna recusada: vale se estiver livre e com um beacon,
                // respiradouro ou fumarola (lista dword_5737D0).
                if (y < 0 || y >= g_gameMap.height() || g_gameMap.cellOccupant(x, y)) { ok = false; continue; }
                bool site = false;
                for (size_t k = 1; k < g_units.size() && !site; ++k) {
                    const UnitRecord& s = g_units[k];
                    if (s.id == -1 || (s.type != 0x51 && s.type != 0x52 && s.type != 0x53)) continue;
                    site = ((unitOX(s) >> 5) & wmask()) == x && (unitOY(s) >> 5) == y;
                }
                if (!site) ok = false;
            }
        if (ok) { u.field22 = 2; u.field3C = cost; return; }
        if (ownerOf(u) == TethysGame::localPlayer())
            postMessage(unitOX(u), unitOY(u), "Building command not successful", 0x9E);
        u.action = 0; u.field22 = 0;
        return;
    }
    if (u.field23 == 2 && (u.type == mapRoboMiner || u.type == mapGeoCon)) {   // loc_43BA42
        soundPlayAt(unitOX(u), unitOY(u), 0x42);
        killUnit(id, 1);                                  // sub_43A890(1): o veículo vira a mina
        UnitRecord& v = g_units[id];
        v.flags |= UNIT_DEAD;                             // 0x20000
        v.cargo = 0;
        return;
    }
    if (u.field23 == 1) {                                 // chegou ao canto da obra
        if (u.type != mapConVec) { u.action = 0; return; }
        if (!turnTo(u, 0x60)) return;                     // espera virar
        const StructSheetDesc* d = structSheet(static_cast<int16_t>(u.cargo));
        if (!d) { u.action = 0; u.field22 = 0; return; }
        pfSetContext(id);                                 // sub_438180/sub_438260
        int cost = 0;
        bool ok = true;
        const int mc = u.moveClass;
        for (int i = 0; i < d->sizeX + 2; ++i)
            for (int j = 0; j < d->sizeY + 2; ++j) {
                const int x = (u.buildL + i) & wmask(), y = u.buildT + j;
                if (y >= 0 && y < g_gameMap.height()) {
                    if (TethysGame::cheatFlag(4)) ++cost;  // dword_56DE68
                    else {
                        const int ct = static_cast<int>(g_gameMap.rawCell(x, y) & 0x1F);
                        if (ct != 0x15 && ct != 0x1A) cost += terrainMoveCost(ct, 3) >> 6;   // dword_4DEBA8
                    }
                }
                const bool ring = i == 0 || j == 0 || i == d->sizeX + 1 || j == d->sizeY + 1;
                ok = ok && (ring ? cellOkRing(id, mc, x, y) : cellOkInterior(id, x, y));
            }
        if (ok) { u.field22 = 2; u.field3C = cost; return; }
        if (ownerOf(u) == TethysGame::localPlayer())
            postMessage(unitOX(u), unitOY(u), "Building command not successful", 0x9E);
        u.action = 0; u.field22 = 0;
        return;
    }
    if (u.field23 == 2) {                                 // obra iniciada: espera (sub-estado 0x0A)
        // (com o atalho dword_56DE6C a espera é 1)
        u.field22 = 0x0A;
        u.flags &= ~UNIT_DAMAGEABLE;
        u.cargo = 0;                                      // o kit foi usado
        return;
    }
    if (u.field23 == 0x0A) {                              // obra pronta
        u.field22 = 0; u.action = 0;
        u.flags |= UNIT_DAMAGEABLE;
    }
}

void conVecBuildStart(int id) {                           // sub_43DE50 case 2
    UnitRecord& u = g_units[id];
    int type = static_cast<int16_t>(u.cargo);
    const bool miner = u.type == mapRoboMiner || u.type == mapGeoCon;
    if (miner) {                                          // loc_43E013 / loc_43E064
        type = u.type == mapRoboMiner ? 0x15 : 0x2C;      // mina / Geothermal Plant
        g_gameMap.clearUnitOccupancy((u.pixelX >> 5) & wmask(), u.pixelY >> 5);   // and 0xF800FFFF
    }
    // Centro do retângulo (área + anel) em tiles.
    int dx = u.buildR - u.buildL - 1;
    if (dx < 0) dx += g_gameMap.width() - 1 + 1;          // (largura interna − 1 no original)
    const int x = ((dx * 16 + (u.buildL + 1) * 32) >> 5) & wmask();
    const int y = ((u.buildB - u.buildT - 1) * 16 + (u.buildT + 1) * 32) >> 5;
    const int bld = placeBuilding(type, x, y, ownerOf(u));   // sub_4369E0
    if (!bld) return;                                     // (fica no sub-estado 2 e tenta de novo)
    UnitRecord& cv = g_units[id];                         // (placeBuilding pode realocar)
    g_units[bld].cargo = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(cv.kitWeapon)));   // arma do kit
    cv.kitWeapon = 0;
    // sub_4030E0: o prédio entra no grupo [+0x6A] (antes da ação 0x21).
    // (revelação do mapa dword_574C88 — pendente)
    if (cv.buildGroup != -1) aiTakeUnit(cv.buildGroup, bld);
    structureStartBuild(bld, id);                         // ação 0x21, duração com o [+0x3C] da ConVec
    UnitRecord& c2 = g_units[id];
    c2.field3C = g_units[bld].buildTime;                  // [esi+3Ch] = [edi+5Ch]
    if (!miner) soundPlayAt(unitOX(g_units[bld]), unitOY(g_units[bld]), 3);   // (sub_42F000 pendente)
    c2.field23 = c2.field22;
    c2.field22 = 0;
}

} // namespace op2
