#include "earthwork.h"
#include "../audio/sound.h"

#include <cstdlib>

#include "building_sheet.h"
#include "command.h"
#include "construction.h"
#include "game_map.h"
#include "messages.h"
#include "pathfinder.h"
#include "player.h"
#include "terrain_cost.h"
#include "tethys_game.h"
#include "unit.h"
#include "walls.h"
#include "structure.h"
#include "weapon.h"

namespace op2 {

static bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
static int wmask() { return g_gameMap.width() - 1; }
static int wrapD(int d) {                                    // x com wrap (meia largura)
    const int W = g_gameMap.width();
    if (d < -W / 2) d += W; else if (d > W / 2) d -= W;
    return d;
}
static bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}

// sub_438A70: (x,y) é a doca de algum prédio?
static bool isDock(int x, int y) {
    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& b = g_units[i];
        if (b.id == -1 || !(b.flags & UNIT_BUILDING)) continue;
        const StructSheetDesc* d = structSheet(b.type);
        if (!d) continue;
        const bool eden = ownerOf(b) < kMaxPlayers && g_playerData[ownerOf(b)].isEden;
        const int dock = eden ? d->dockLocE : d->dockLocP;
        if (!dock) continue;
        const int cx = b.pixelX >> 5, cy = b.pixelY >> 5;
        const int dx = (cx - (d->sizeX >> 1) + dock - 1) & wmask();
        const int dy = cy - (d->sizeY >> 1) + d->sizeY - 1;
        if (dx == x && dy == y) return true;
    }
    return false;
}

// sub_438D70: vizinho de um muro novo.
static bool wallNeighborOk(int x, int y) {
    if (y < -1 || y >= g_gameMap.height()) return false;
    if (y == -1) return true;
    const uint32_t c = g_gameMap.rawCell(x, y);
    if (c & 0xAFFF0000u) {
        const int ct = static_cast<int>(c & 0x1F);
        if (ct >= 0x17 && ct <= 0x19) return true;           // outro muro
        const int occ = static_cast<int>((c & 0x7FF0000u) >> 16);
        if (occ == g_pf.unitId) return (c & 0xA8000000u) == 0;
        if (occ && valid(occ) && (g_units[occ].flags & UNIT_BUILDING))
            return g_units[occ].type == 0x17;                // Guard Post
    }
    return (c & 0xA8000000u) == 0;
}

bool wallCellOk(int x, int y, int wallType) {                // sub_438C60
    if (y < 0 || y >= g_gameMap.height()) return false;
    x &= wmask();
    const uint32_t c = g_gameMap.rawCell(x, y);
    if ((c & 0x1F) == 0x1A) return false;
    if (c & 0xAFFF0000u) {
        const int occ = static_cast<int>((c & 0x7FF0000u) >> 16);
        if (occ != g_pf.unitId || (c & 0xA8000000u)) return false;
    }
    if (isDock(x, y)) return false;
    if (!pfCanEnter(x, y)) return false;
    if (wallType == 0x11) return true;
    static const int kN[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};   // unk_4E0C10
    for (const auto& n : kN)
        if (!wallNeighborOk((x + n[0]) & wmask(), y + n[1])) return false;
    return true;
}

bool dozeCellOk(int x, int y) {                              // sub_438E50
    if (y < 0 || y >= g_gameMap.height()) return false;
    x &= wmask();
    if (g_gameMap.rawCell(x, y) & 0xAFFF0000u) return false;
    if (isDock(x, y)) return false;
    return pfCanEnter(x, y) != 0;
}

// sub_40A300 (vt[0x90] do Robo-Dozer).
static bool dozerCan(int x, int y) {
    const uint32_t c = g_gameMap.rawCell(x & wmask(), y);
    if (c & 0x80000000u) return false;
    const int ct = static_cast<int>(c & 0x1F);
    if (!terrainMoveCost(ct, g_pf.moveClass)) return false;
    const int occ = static_cast<int>((c & 0x7FF0000u) >> 16);
    if (occ) {
        if (!valid(occ) || !(g_units[occ].flags & UNIT_VEHICLE)) return false;
        if (!pfOccupantPassable(occ, g_pf.unitId)) return false;
    }
    return ct != 0x15 && ct != 0x1A;
}

// vt[0x90] do veículo: muro/tubo (sub_40A470) ou dozer (sub_40A300).
static bool canWork(const UnitRecord& u, int x, int y) {
    if (y < 0 || y >= g_gameMap.height()) return false;
    if (u.type == mapRoboDozer) return dozerCan(x, y);
    return wallCellOk(x, y, static_cast<int16_t>(u.cargo));
}

// Manda o veículo à célula (vt[0x7C] com o modo): 2 = já está no objetivo, 1 = andando.
static int goTo(int id, int x, int y, int mode) {
    UnitRecord& u = g_units[id];
    pfSetContext(id);
    const int gx = mode == 3 ? (x - 1) & wmask() : x;       // modo 3: alvo em x − 1
    pfSetGoal(mode, gx, y, 0);
    const int ux = (u.pixelX >> 5) & wmask(), uy = u.pixelY >> 5;
    if (pfMoveCost(ux, uy) < 0 || (ux == gx && uy == y)) return 2;
    u.hasOrder = true;
    u.order = OrderBlock{};
    u.order.count = 1;
    u.order.countMinus1 = 0;
    u.order.waypoints.assign(1, packLocation(x, y));
    u.order.flags |= 1;
    u.order.multiFlag = mode;                                // modo do último ponto
    u.order.field40 = 0;
    u.clearRoute();
    return 1;
}

// sub_409DB0: percorre a área a partir de (x,y) até achar uma célula de trabalho.
// cur != nullptr (muro): grava a célula e para ao lado; nullptr (dozer): vai em cima.
static int nextCell(int id, int x, int y, uint8_t& code, int16_t* curX, int16_t* curY) {
    UnitRecord& u = g_units[id];
    const int x1 = u.buildL, y1 = u.buildT, x2 = u.buildR, y2 = u.buildB;
    int dir = code & 6, step = 1, limit = 0;
    switch (dir) {
        case 0: step = -1; limit = y1; break;
        case 2: step = -1; limit = x1; break;
        case 4: step = 1; limit = y2; break;
        case 6: step = 1; limit = x2; break;
    }
    if (code & 0x80) dir = (dir + 4) & 6;
    pfSetContext(id);
    for (int guard = 0; !canWork(u, x, y); ++guard) {
        if (guard > 0x10000) return 0;
        switch (dir) {
            case 0:
                if (x2 == x) { if (limit == y) return 0; y += step; dir = 4; code ^= 0x80; }
                else x = (x + 1) & wmask();
                break;
            case 2:
                if (y1 == y) { if (limit == x) return 0; x = (x + step) & wmask(); dir = 6; code ^= 0x80; }
                else --y;
                break;
            case 4:
                if (x1 == x) { if (limit == y) return 0; y += step; dir = 0; code ^= 0x80; }
                else x = (x - 1) & wmask();
                break;
            case 6:
                if (y2 == y) { if (limit == x) return 0; x = (x + step) & wmask(); dir = 2; code ^= 0x80; }
                else ++y;
                break;
        }
    }
    int ret;
    if (curX) {
        const int ux = (u.pixelX >> 5) & wmask(), uy = u.pixelY >> 5;
        const bool adj = ux == x ? std::abs(y - uy) == 1 : (uy == y && std::abs(wrapD(x - ux)) == 1);
        ret = adj ? 2 : goTo(id, x, y, 3);
        *curX = static_cast<int16_t>(x); *curY = static_cast<int16_t>(y);
    } else {
        ret = goTo(id, x, y, 0);
    }
    return ret;
}

// sub_40A0A0: canto mais próximo (distância maior + menor/2) → código de percurso.
static int startArea(int id, uint8_t& code, int16_t* curX, int16_t* curY) {
    const UnitRecord& u = g_units[id];
    const int ux = u.pixelX >> 5, uy = u.pixelY >> 5;
    const int halfW = g_gameMap.width() / 2;
    auto dist = [&](int cx, int cy) {
        int dx = std::abs(cx - ux), dy = std::abs(cy - uy);
        if (dx > halfW) dx -= halfW;                         // (como o original)
        return dy < dx ? dx + dy / 2 : dy + dx / 2;
    };
    const int x1 = u.buildL, y1 = u.buildT, x2 = u.buildR, y2 = u.buildB;
    int best = dist(x1, y1), sx = x1, sy = y1;
    code = 6;
    if (dist(x2, y2) < best) { best = dist(x2, y2); code = 2; sx = x2; sy = y2; }
    if (dist(x2, y1) < best) { best = dist(x2, y1); code = 4; sx = x2; sy = y1; }
    if (dist(x1, y2) < best) { code = 0; sx = x1; sy = y2; }
    return nextCell(id, sx, sy, code, curX, curY);
}

// ---- Pacotes -------------------------------------------------------------------
bool sendBuildWall(const std::vector<int>& sel, int x1, int y1, int x2, int y2, int wallType) {
    const int me = TethysGame::localPlayer();
    // sub_452770: alguma célula do retângulo aceita o tipo?
    bool any = false;
    for (int id : sel) {
        if (!valid(id)) continue;
        pfSetContext(id);
        int xe = x2 - 1; if (xe < x1) xe += g_gameMap.width();
        for (int x = x1; x <= xe && !any; ++x)
            for (int y = y1; y <= y2 - 1 && !any; ++y)
                any = wallCellOk(x & wmask(), y, wallType);
        break;
    }
    if (!any) return false;
    soundPlay(0x56);
    for (int id : sel) {                                     // case 7 → sub_4234C0
        if (!valid(id) || ownerOf(g_units[id]) != me) continue;
        earthworkWallOrder(id, x1, y1, x2, y2, wallType);
    }
    return true;
}

bool earthworkWallOrder(int id, int x1, int y1, int x2, int y2, int wallType) {
    if (!valid(id)) return false;
    UnitRecord& u = g_units[id];
    if (!commandable(u)) return false;
    u.action = 7; u.field20 = 1; u.field3C = 0;
    u.buildL = static_cast<int16_t>(x1 & wmask()); u.buildT = static_cast<int16_t>(y1);
    u.buildR = static_cast<int16_t>((x2 - 1) & wmask()); u.buildB = static_cast<int16_t>(y2 - 1);
    u.cargo = static_cast<uint16_t>(wallType);
    return true;
}

bool sendBulldoze(const std::vector<int>& sel, int x1, int y1, int x2, int y2) {
    if (sel.empty() || !valid(sel[0])) return false;
    // sub_453960: alguma célula serve?
    bool any = false;
    pfSetContext(sel[0]);
    int xe = x2 - 1; if (xe < x1) xe += g_gameMap.width();
    for (int x = x1; x <= xe && !any; ++x)
        for (int y = y1; y <= y2 - 1 && !any; ++y) any = dozeCellOk(x & wmask(), y);
    if (!any) return false;
    // sub_453780: linha de largura 1 e comprimento > 1 → pacote vazio; senão
    // x2/y2 − 1 (ou a célula do 1º ponto).
    const int dx = std::abs(wrapD(x1 - x2)), dy = std::abs(y2 - y1);
    if ((dx == 0 && dy > 1) || (dy == 0 && dx > 1)) return true;
    int rx = x2, ry = y2;
    if (dx && dy) { rx = (x2 - 1) & wmask(); ry = y2 - 1; }
    else { rx = x1; ry = y1; }
    soundPlay(0x56);
    UnitRecord& u = g_units[sel[0]];                         // case 1: só a 1ª unidade
    if (ownerOf(u) != TethysGame::localPlayer() || !commandable(u)) return true;
    u.action = 1; u.field20 = 1; u.field3C = 0;              // sub_423780
    u.buildL = static_cast<int16_t>(x1 & wmask()); u.buildT = static_cast<int16_t>(y1);
    u.buildR = static_cast<int16_t>(rx); u.buildB = static_cast<int16_t>(ry);
    return true;
}

void wallDragConstrain(int sx, int sy, int& ex, int& ey) {   // sub_41C410
    const int W = g_gameMap.width() * 32;
    const int mask = W - 1;
    sx &= mask; ex &= mask;
    int adx = std::abs(sx - ex);
    if ((W + 1) / 2 < adx) adx = W - adx;
    const int ady = std::abs(sy - ey);
    if (ady < adx) {                                         // horizontal
        ey = sy;
        if ((adx & ~0x1F) < 0x100) return;
        int d = std::abs(ex - sx);
        const bool wrapped = d > (W + 1) / 2;
        const bool right = ex > sx;
        ex = (right != wrapped) ? sx + 0xE0 : sx - 0xF1;
        return;
    }
    ex = sx;                                                 // vertical
    if ((ady & ~0x1F) < 0x100) return;
    ey = ey > sy ? sy + 0xE0 : sy - 0xF1;
}

// ---- Ações ---------------------------------------------------------------------
// Retoma: [+0x23] = [+0x22], [+0x22] = 1 (ou já no objetivo).
static void moveOn(UnitRecord& u, int r) {
    if (r == 2) { u.field23 = 1; u.field22 = 0; u.field20 = 0; return; }
    u.field23 = u.field22; u.field22 = 1; u.field20 = 1;
}

bool sendRemoveWall(const std::vector<int>& sel, int tileX, int tileY) {   // sub_452B60 + sub_452D10
    tileX &= wmask();
    if (tileY < 0 || tileY >= g_gameMap.height()) return false;
    const int ct = static_cast<int>(g_gameMap.rawCell(tileX, tileY) & 0x1F);
    bool ok = ct >= 0x17 && ct <= 0x19;
    if (!ok && ct == 0x1A) {                                 // tubo: sem prédio no ponto
        ok = true;
        for (size_t i = 1; i < g_units.size() && ok; ++i) {
            const UnitRecord& b = g_units[i];
            if (b.id == -1 || !(b.flags & UNIT_BUILDING)) continue;
            int w, h; buildingFootprint(b.type, w, h);
            const int l = ((b.pixelX >> 5) - (w >> 1)) & wmask(), t = (b.pixelY >> 5) - (h >> 1);
            if (tileY >= t && tileY < t + h && ((tileX - l) & wmask()) < w) ok = false;
        }
    }
    if (!ok) return false;
    soundPlay(0x56);
    const int me = TethysGame::localPlayer();
    for (int id : sel) {                                     // case 8 → vt[0x94]
        if (!valid(id)) continue;
        UnitRecord& u = g_units[id];
        if (ownerOf(u) != me || !commandable(u) || u.type != mapEarthworker) continue;
        u.action = 8; u.field20 = 1; u.field3C = 0;         // sub_423500
        u.hasOrder = false;
        u.buildL = u.buildR = static_cast<int16_t>(tileX);
        u.buildT = u.buildB = static_cast<int16_t>(tileY);
    }
    return true;
}

void removeWallTick(int id) {                                // jpt_43B312 case 8
    UnitRecord& u = g_units[id];
    const int bx = u.buildL & wmask(), by = u.buildT;
    if (u.field23 == 1) {
        const int ux = (u.pixelX >> 5) & wmask(), uy = u.pixelY >> 5;
        const bool adj = bx == ux ? std::abs(uy - by) == 1 : (uy == by && std::abs(wrapD(bx - ux)) == 1);
        if (!adj) { u.action = 0; u.field22 = 0; }           // (o original segue mesmo assim)
        if (!unitTurnTo(u, dirOfVec(wrapD(bx - ux), by - uy))) return;
        int wait;
        if (TethysGame::cheatFlag(5)) wait = 2;              // dword_56DE6C
        else {
            const VehPlayerStats* vs = vehicleSheet(mapEarthworker, ownerOf(u));
            wait = (vs ? vs->productionRate : 0) * 375 / 128 / 2;
        }
        u.field3C = wait > 0 ? wait : 1;
        if ((g_gameMap.rawCell(bx, by) & 0x1F) == 0x1A) {    // tubo: poeira (0x72, anim 0x619)
            const int e = createAmbientAnim(u.pixelX, u.pixelY + 1, 0x619, 1, 10);
            if (e) { g_units[e].action = ACT_DIE; g_units[e].field20 = 1; g_units[e].field3C = u.field3C; }
        }
        g_units[id].field22 = 0x0A;
        return;
    }
    if (u.field23 != 0x0A) return;
    u.action = 0;
    if (destroyWallOrTube(bx, by) != 1) return;              // sub_4A25F0
    UnitRecord& v = g_units[id];
    v.damage = static_cast<int16_t>(v.damage + 100);         // [+0x1E] += 0x64
    const VehPlayerStats* vs = vehicleSheet(v.type, (v.owner >> 4) & 0x0F);
    const int hp = vs ? vs->hitPoints : 0;
    if (hp <= v.damage) {
        v.damage = static_cast<int16_t>(hp);
        v.flags |= UNIT_DEAD;                                // 0x20000
        killUnit(id, 5);                                     // sub_43A890(5)
    }
}

void earthworkStart(int id) {                                // jpt_43AD56 casos 1 / 7 / 8
    UnitRecord& u = g_units[id];
    if (u.action == 8) {                                     // case 8: ao lado da célula (modo 3)
        const int r = goTo(id, u.buildL & wmask(), u.buildT, 3);
        moveOn(g_units[id], r);
        g_units[id].field23 = r == 2 ? 1 : 0;
        return;
    }
    const int r = u.action == 7 ? startArea(id, u.wallCode, &u.wallCurX, &u.wallCurY)
                                : startArea(id, u.dozeCode, nullptr, nullptr);
    UnitRecord& v = g_units[id];
    if (r == 0) {                                            // nada a fazer
        v.field22 = 0; v.field23 = (v.action == 1) ? 1 : 0;
        v.action = 0; v.field20 = 0;
        return;
    }
    if (r == 2) { v.field23 = 1; v.field22 = 0; v.field20 = 0; return; }
    v.field22 = 1; v.field20 = 1;                            // (port: [+0x20] mantém o movimento)
}

void wallBuildTick(int id) {                                 // jpt_43B312 case 7
    UnitRecord& u = g_units[id];
    bool built = false;
    if (u.field23 == 1) {
        const int bx = u.wallCurX & wmask(), by = u.wallCurY;
        const int ux = (u.pixelX >> 5) & wmask(), uy = u.pixelY >> 5;
        const bool adj = bx == ux ? std::abs(uy - by) == 1 : (uy == by && std::abs(wrapD(bx - ux)) == 1);
        if (adj) {
            if (!unitTurnTo(u, dirOfVec(wrapD(bx - ux), by - uy))) return;   // sub_45C4F0 + sub_44C410
            pfSetContext(id);
            if (buildCellOk(id, bx, by)) {                   // sub_4389E0
                const int type = static_cast<int16_t>(u.cargo);
                const int p = ownerOf(u);
                const StructSheetDesc* d = wallSheet(type);
                PlayerData& pd = g_playerData[p];
                const int rare = d ? d->player[p].rareOre : 0, common = d ? d->player[p].commonOre : 0;
                if (pd.rareOre < rare || pd.ore < common) { u.action = 0; u.field22 = 0; return; }
                pd.rareOre -= rare;
                pd.ore -= common;
                if (type != 0x11) {                          // muro: reserva a célula
                    g_gameMap.setRawCell(bx, by, g_gameMap.rawCell(bx, by) | 0x80000000u);
                    u.field3C = 0;
                } else {
                    u.field3C = terrainMoveCost(static_cast<int>(g_gameMap.rawCell(bx, by) & 0x1F), 3) * 3 >> 6;
                }
                if (TethysGame::cheatFlag(5)) u.field3C = 1;  // dword_56DE6C
                else {
                    const VehPlayerStats* vs = vehicleSheet(mapEarthworker, p);
                    const int bt = d ? d->player[p].buildTime : 0;
                    u.field3C = (bt + u.field3C) * (vs ? vs->productionRate : 0) / 128;
                    if (u.field3C <= 0) u.field3C = 1;
                }
                u.field22 = 0x0A;
                return;
            }
        }
    } else if (u.field23 == 0x0A) {                          // sub_4235E0: assenta
        const int bx = u.wallCurX & wmask(), by = u.wallCurY;
        const int type = static_cast<int16_t>(u.cargo);
        pfSetContext(id);
        if (type == 0x11 && !wallCellOk(bx, by, 0x11)) {     // tubo: ficou ocupado → devolve
            const StructSheetDesc* d = wallSheet(type);
            const int p = ownerOf(u);
            if (d) { g_playerData[p].rareOre += d->player[p].rareOre; g_playerData[p].ore += d->player[p].commonOre; }
        } else {
            if (type != 0x11)                                // tira a reserva antes de assentar
                g_gameMap.setRawCell(bx, by, g_gameMap.rawCell(bx, by) & ~0x80000000u);
            createWallOrTube(bx, by, type);
        }
        built = true;
    } else {
        return;
    }
    u.wallCode ^= 0x40;                                       // sub_40A2B0
    const int r = nextCell(id, u.wallCurX & wmask(), u.wallCurY, u.wallCode, &u.wallCurX, &u.wallCurY);
    UnitRecord& v = g_units[id];
    if (r) {
        moveOn(v, r);
        if (built) soundPlayAt(unitOX(v), unitOY(v), 3);
        return;
    }
    v.action = 0; v.wallCurY = -1;                           // [+0x6E] = 0xFFFF
    if (built) soundPlayAt(unitOX(v), unitOY(v), 3);
    if (ownerOf(v) == TethysGame::localPlayer())          // (o original avisa mesmo sem assentar)
        postMessage(unitOX(v), unitOY(v), static_cast<int16_t>(v.cargo) == 0x11 ? "Tube completed" : "Wall completed",
                    static_cast<int16_t>(v.cargo) == 0x11 ? 0xA4 : 0xA3);
}

void bulldozeTick(int id) {                                  // jpt_43B312 case 1
    UnitRecord& u = g_units[id];
    const int ux = (u.pixelX >> 5) & wmask(), uy = u.pixelY >> 5;
    if (u.field23 == 6) {                                    // terminou a espera: terraplana
        u.dozeCode ^= 0x40;
        const int tile = g_gameMap.getTile(LOCATION{ux, uy});
        g_gameMap.setTileAndCellType(ux, uy, g_gameMap.bulldozedTile(tile), GameMap::CELLTYPE_DOZED);   // sub_435330
        const int r = nextCell(id, ux, uy, u.dozeCode, nullptr, nullptr);   // sub_40A2E0
        UnitRecord& v = g_units[id];
        if (r) { moveOn(v, r); return; }
        v.action = 0;
        if (ownerOf(v) == TethysGame::localPlayer())
            postMessage(unitOX(v), unitOY(v), "Bulldozing completed", 0xA5);
        return;
    }
    if (u.field23 != 1) return;
    // Chegou (sub_44BEA0: com 1 ponto, sempre no fim): espera o custo do terreno.
    const uint32_t c = g_gameMap.rawCell(ux, uy);
    u.field23 = u.field22;
    u.field22 = SUB_WAIT;
    if (TethysGame::cheatFlag(4)) { u.field3C = 1; return; } // dword_56DE68 (loc_43DCDF)
    int t = terrainMoveCost(static_cast<int>(c & 0x1F), 3) * 3 >> 6;
    const VehPlayerStats* vs = vehicleSheet(mapRoboDozer, ownerOf(u));
    t = t * (vs ? vs->productionRate : 0) / 128;
    u.field3C = t > 0 ? t : 1;
}

} // namespace op2
