#include "garage.h"

#include "../audio/sound.h"
#include "building_sheet.h"
#include "colony.h"
#include "docking.h"
#include "game_map.h"
#include "messages.h"
#include "player.h"
#include "structure.h"
#include "tethys_game.h"
#include "unit.h"

namespace op2 {

static bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
static int statsOf(const UnitRecord& r) { return (r.owner >> 4) & 0x0F; }
static void dropRef(int id) {
    if (id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].refCount) --g_units[id].refCount;
}
// vt[0x74] (aceita comando).
static bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}

int garageBays(const UnitRecord& g) {
    const StructSheetDesc* d = structSheet(g.type);
    const int n = d ? d->player[statsOf(g)].storageBays : 0;
    return n < 0 ? 0 : n > 6 ? 6 : n;
}

int garageVehicle(int gid, int bay) {
    if (!valid(gid) || bay < 0 || bay >= 6) return 0;
    return g_units[gid].garageSlot[bay];
}

int garageCount(int gid) {                                    // sub_41C9E0
    const UnitRecord& g = g_units[gid];
    int n = 0;
    for (int i = 0; i < garageBays(g); ++i) if (g.garageSlot[i]) ++n;
    return n;
}

bool garageBusy(int gid) {                                    // sub_41D3B0
    const UnitRecord& g = g_units[gid];
    for (int i = 0; i < garageBays(g); ++i) {
        const int v = g.garageSlot[i];
        if (v && (g_units[v].field22 == 0x0A || g_units[v].field22 == 1)) return true;
    }
    return false;
}

// sub_44CEF0: o veículo sai da lista e do mapa ([+4] = −2), solta referências
// (sub_439EB0), sai das listas do jogador (sub_439C00); sub-estado 1, 0x20000,
// [+0x5E] = tick.
static void enterGarage(int vid) {
    UnitRecord& v = g_units[vid];
    unitListRemove(vid);                                      // sub_439CB0
    const int cx = v.pixelX >> 5, cy = v.pixelY >> 5;         // sub_439CE0
    if (g_gameMap.ok() && g_gameMap.cellOccupant(cx, cy) == vid) g_gameMap.clearUnitOccupancy(cx, cy);
    if (v.lastAttacker) { dropRef(v.lastAttacker); v.lastAttacker = 0; }   // sub_439EB0
    if ((v.flags & UNIT_VEHICLE) && (v.guardHi & 0x8000)) { dropRef(v.guardLo); v.guardHi = 0x7FFF; }
    if ((v.flags & (UNIT_VEHICLE | UNIT_TURRET_BLD)) && !(v.flags & 0x100u)) {
        if (v.field4A & 0x8000) { dropRef(v.orderLo); v.field4A = 0x7FFF; }
        if (v.field4E & 0x8000) { dropRef(v.target4C); v.field4E = 0x7FFF; }
    }
    colonyVehicleRemoved(vid);                                // sub_439C00
    v.field22 = 1;
    v.flags |= UNIT_DEAD;
    v.dockTick = static_cast<int16_t>(TethysGame::tick());
}

// sub_44CE60(v, 0): volta à lista por Y, ocupa a célula, às listas do jogador;
// armado perde os alvos; tira o 0x20000.
static void leaveGarage(int vid) {
    UnitRecord& v = g_units[vid];
    unitListInsert(vid);                                      // sub_43A1C0
    if (g_gameMap.ok()) g_gameMap.setUnitOccupancy(v.pixelX >> 5, v.pixelY >> 5, vid);
    colonyVehicleAdded(vid);                                  // sub_439FE0
    if (v.flags & UNIT_ARMED) { v.field4E = 0x7FFF; v.field4A = 0x7FFF; v.guardHi = 0x7FFF; }
    v.flags &= ~UNIT_DEAD;
}

void garageBay(int gid, int bay) {                            // sub_41C5F0
    if (!valid(gid) || bay < 0 || bay >= 6) return;
    UnitRecord& g = g_units[gid];
    if (garageBusy(gid)) return;
    const int docked = dockedVehicle(gid);                    // sub_483010
    if (!docked) {                                            // solta o da baia
        const int vid = g.garageSlot[bay];
        if (!vid) return;
        UnitRecord& v = g_units[vid];
        v.flags &= ~UNIT_EMP;
        v.empTicks = 0;
        int x, y;
        if (dockCell(gid, x, y)) {                            // sub_482F80
            const int mx = x & (g_gameMap.width() - 1);
            if (g_gameMap.rawCell(mx, y) & 0x7FF0000u) return;   // doca ocupada
            g_units[gid].flags |= 0x1000000u;
            g_gameMap.setRawCell(mx, y, g_gameMap.rawCell(mx, y) | 0x80000000u);
            g_gameMap.setUnitOccupancy(mx, y, gid);
        }
        UnitRecord& v2 = g_units[vid];
        v2.field22 = 0x0A;
        v2.dockTick = static_cast<int16_t>(TethysGame::tick());
        soundPlayAt(unitOX(g_units[gid]), unitOY(g_units[gid]), 0x3D);
        return;
    }
    if (g.garageSlot[bay]) return;                            // guarda o da doca
    UnitRecord& d = g_units[docked];
    if (d.type == mapSpider || d.type == mapScorpion) return;
    if (d.action) d.action = 0;
    ++d.refCount;
    g_units[gid].garageSlot[bay] = docked;
    enterGarage(docked);
    UnitRecord& g2 = g_units[gid];
    if (g2.action != 0x1B && g2.action != 0x12 && g2.action == 0) {
        g2.action = 0x1B; g2.field20 = 1; g2.field3C = 0;
    }
}

void garageTick(int gid) {                                    // sub_41D200 (depois de sub_408930)
    UnitRecord& g = g_units[gid];
    const bool eden = ownerOf(g) < kMaxPlayers && g_playerData[ownerOf(g)].isEden;   // [+0x44] & 0x1000
    const int lim = eden ? 0x1E : 0x37;
    for (int i = 0; i < garageBays(g); ++i) {
        const int vid = g_units[gid].garageSlot[i];
        if (!vid) continue;
        UnitRecord& v = g_units[vid];
        const int t = static_cast<int>((TethysGame::tick() - static_cast<int>(v.dockTick)) & 0xFFFF);   // sub_44DFE0
        if (v.field22 == 0x0A) {                              // saindo
            if (t <= lim) continue;
            if (g_units[gid].flags & 0x1000000u) {
                g_units[gid].flags &= ~0x1000000u;
                int x, y;
                if (dockCell(gid, x, y)) {
                    const int mx = x & (g_gameMap.width() - 1);
                    g_gameMap.setRawCell(mx, y, g_gameMap.rawCell(mx, y) & 0x7FFFFFFFu);
                    g_gameMap.clearUnitOccupancy(mx, y);
                }
            }
            g_units[vid].field22 = 0;
            leaveGarage(vid);
            dropRef(vid);
            g_units[gid].garageSlot[i] = 0;
        } else if (v.field22 == 1) {                          // entrando
            if (t <= lim) continue;
            v.field22 = 0;
            soundPlayAt(unitOX(g), unitOY(g), 0x3D);
        }
    }
}

// sub_41CA50: um passo de conserto. true se algum veículo ainda tem dano (ou
// está entrando/saindo).
static bool garageRepair(int gid) {
    const UnitRecord& g = g_units[gid];
    const int p = ownerOf(g);
    PlayerData& pd = g_playerData[p];
    const StructSheetDesc* gd = structSheet(g.type);
    const int maxRepair = gd ? gd->player[statsOf(g)].productionCapacity : 0;   // bloco+0x34
    bool any = false, shown = false;
    for (int i = 0; i < garageBays(g); ++i) {
        const int vid = g_units[gid].garageSlot[i];
        if (!vid) continue;
        UnitRecord& v = g_units[vid];
        if (v.field22 == 0x0A || v.field22 == 1) { any = true; continue; }
        if (!v.damage) continue;
        int cnt = v.damage < maxRepair ? v.damage : maxRepair;
        const VehPlayerStats* vs = vehicleSheet(v.type, statsOf(v));
        int common = vs ? vs->commonOre : 0, rare = vs ? vs->rareOre : 0;
        if (v.flags & UNIT_ARMED) {
            if (const WeaponSheetDesc* w = weaponSheet(static_cast<int16_t>(v.cargo))) {
                common += w->player[statsOf(v)].commonOre;
                rare += w->player[statsOf(v)].rareOre;
            }
        }
        const int hp = vs ? vs->hitPoints : 0;
        if (hp <= 0) continue;
        int cost = common * cnt / (hp * 2);
        if (cost > pd.ore) { cnt = common ? hp * pd.ore * 2 / common : cnt; cost = pd.ore; }
        if (rare) {
            int rc = rare * cnt / (hp * 2);
            if (rc > pd.rareOre) {                            // (o custo comum vira o raro recalculado)
                cnt = hp * pd.rareOre * 2 / rare;
                cost = rare * cnt / (hp * 2);
                rc = pd.rareOre;
            }
            pd.rareOre -= rc;
        }
        pd.ore -= cost;
        v.damage = static_cast<int16_t>(v.damage - cnt);
        if (v.damage == 0 && p == TethysGame::localPlayer() && !shown) {
            postMessage(unitOX(g), unitOY(g), "Vehicle Repaired", 0x6B);
            shown = true;
        }
        if (any || v.damage != 0) any = true;
    }
    return any;
}

void garageRepairStart(int gid) {                             // jpt_43AD56 case 27
    UnitRecord& g = g_units[gid];
    if (g.field22 != 7) {
        g.field22 = 7;
        const StructSheetDesc* d = structSheet(g.type);
        const int v = d ? d->player[statsOf(g)].storageCapacity : 0;   // bloco+0x30
        g.field3C = TethysGame::cheatFlag(5) ? 1 : v * 32 / 128;   // dword_56DE6C
    }
    g.field20 = 0;
}

void garageRepairTick(int gid) {                              // jpt_43B312 case 27
    UnitRecord& g = g_units[gid];
    if (garageRepair(gid)) { g.action = 0x1B; g.field20 = 1; g.field3C = 0; }
    else g.action = 0;
}

bool garageBayAvailable(int gid, int bay) {                   // sub_456BC0, ramo 0x29
    if (!valid(gid) || bay < 0 || bay >= garageBays(g_units[gid])) return false;
    const int d = dockedVehicle(gid);
    if (d) {
        const UnitRecord& v = g_units[d];
        if (v.type == mapSpider || v.type == mapScorpion) return false;
        if (!commandable(v) || (v.flags & 0x40000u)) return false;
    }
    if (garageBusy(gid)) return false;
    const int s = g_units[gid].garageSlot[bay];
    return d ? s == 0 : s != 0;
}

} // namespace op2
