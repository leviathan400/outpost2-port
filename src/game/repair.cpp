#include "repair.h"
#include "../audio/sound.h"
#include "ai_group.h"

#include <cstdlib>
#include <cstring>

#include "building_sheet.h"
#include "colony.h"
#include "command.h"
#include "commands.h"
#include "construction.h"
#include "game_map.h"
#include "messages.h"
#include "player.h"
#include "production.h"
#include "structure.h"
#include "tethys_game.h"
#include "unit.h"
#include "weapon.h"

namespace op2 {

namespace {

bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
bool alive(int id) { return valid(id) && !(g_units[id].flags & UNIT_DEAD); }
int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
int statOwner(const UnitRecord& r) { return (r.owner >> 4) & 0x0F; }
void addRef(int id) { if (valid(id)) ++g_units[id].refCount; }
void dropRef(int id) { if (valid(id) && g_units[id].refCount) --g_units[id].refCount; }

int blockInt(int type, int player, int off) {
    const uint8_t* b = unitStatBlock(type, player);
    int v = 0;
    if (b) std::memcpy(&v, b + off, 4);
    return v;
}
// Production Rate do bloco (+0x2C) * 32 / 128 (cdq; and 7Fh; sar 7).
int rateWait(const UnitRecord& u) {
    const int v = blockInt(u.type, statOwner(u), 0x2C) << 5;
    return (v + ((v >> 31) & 0x7F)) >> 7;
}
// vt[0x74] do veículo (sub_44B620).
bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}

// sub_423C70.
void orderRepair(int id, int type, const std::vector<uint32_t>& wps, uint32_t tgt) {
    UnitRecord& u = g_units[id];
    const int t = static_cast<int16_t>(tgt & 0xFFFF);
    auto cancel = [&]() { u.field20 = 1; u.action = 0; u.field3C = 0; };
    if (!alive(t)) { cancel(); return; }
    const UnitRecord& tr = g_units[t];
    if (type == 0x1D && (tr.flags & UNIT_BUILDING)) { cancel(); return; }
    if (type == 0x1E) {
        if ((u.owner ^ tr.owner) & 0x0F) { cancel(); return; }
        if (tr.type == 0x15 || tr.type == 0x16 || tr.type == 0x2A || tr.type == 0x2C) { cancel(); return; }
    }
    u.action = static_cast<uint8_t>(type);
    u.field20 = 1;
    u.field3C = 0;
    if (u.orderTarget() & 0x80000000u) dropRef(u.orderLo);
    u.setOrderTarget(tgt);
    addRef(t);
    u.hasOrder = true;
    u.order = OrderBlock{};
    u.order.waypoints = wps;
    u.order.count = static_cast<int>(wps.size()) + 1;
    u.order.countMinus1 = static_cast<int>(wps.size());
    u.order.multiFlag = 3;                                  // [ordem+0x3C] = 3
    u.order.field40 = 0;
    u.order.waypoints.push_back(unitTargetWaypoint(u, tgt));
    u.clearRoute();
}

// sub_44C490: vira p/ o alvo (direção arredondada a 8).
bool faceTarget(UnitRecord& u, const UnitRecord& t) {
    int dx = unitOX(t) - unitOX(u);
    const int W = g_gameMap.width() * 32;
    if (dx < -W / 2) dx += W; else if (dx > W / 2) dx -= W;
    const uint8_t dir = static_cast<uint8_t>((dirOfVec(dx, unitOY(t) - unitOY(u)) + 8) & 0xE0);
    return unitTurnTo(u, dir);
}

// Ao lado: o tile do alvo mais próximo (vt[0x6C]) a até 1 tile.
bool adjacent(const UnitRecord& u, const UnitRecord& t) {
    int ox, oy;
    const int ux = unitOX(u) >> 5, uy = unitOY(u) >> 5;
    unitNearestTile(t, ux, uy, ox, oy);
    return std::abs(ux - ox) <= 1 && std::abs(uy - oy) <= 1;
}

// sub_44CF40: um passo de reparo; true se o dano mudou.
bool repairStep(int id) {
    UnitRecord& u = g_units[id];
    if (!(u.orderTarget() & 0x80000000u)) return false;
    const int ti = u.orderLo;
    if (!alive(ti)) return false;
    UnitRecord& t = g_units[ti];
    if (!adjacent(u, t)) return false;
    const int amount = blockInt(u.type, statOwner(u), 0x04);  // Repair Amt
    const int dmg = t.damage;
    const int sp = statOwner(t);
    const int hp = blockInt(t.type, sp, 0x00);
    int fix = 0;
    if (t.flags & UNIT_BUILDING) {
        fix = dmg < amount ? dmg : amount;
    } else if (t.flags & UNIT_VEHICLE) {
        if (t.field22 == 0x0D) return false;
        const int half = hp >> 1;
        if (half >= dmg) fix = 0;
        else fix = (dmg - half) < amount ? dmg - half : amount;
        fix = static_cast<int16_t>(fix);
    }
    int common = blockInt(t.type, sp, 0x0C), rare = blockInt(t.type, sp, 0x10);
    if ((t.type >= 5 && t.type <= 7) || t.type == 0x17) {         // + a arma
        const int w = static_cast<int16_t>(t.cargo);
        common += blockInt(w, sp, 0x0C);
        rare += blockInt(w, sp, 0x10);
    }
    PlayerData& d = g_playerData[ownerOf(u)];
    const int div = hp * 2;
    int costC = div ? common * fix / div : 0;
    if (costC && costC > d.ore) {
        fix = common ? hp * d.ore * 2 / common : 0;
        costC = d.ore;
    }
    if (rare) {
        int costR = div ? rare * fix / div : 0;
        if (costR > d.rareOre) {
            fix = hp * d.rareOre * 2 / rare;
            costC = div ? common * fix / div : 0;
            costR = d.rareOre;
        }
        d.rareOre -= costR;
    }
    d.ore -= costC;
    t.damage = static_cast<int16_t>(dmg - fix);
    return t.damage != dmg;
}

// sub_43A3E0(máscara, modo): os armados dos jogadores da máscara esquecem a
// unidade — modo 1: alvo [+0x4C] e ordem [+0x48] (ataque vira STOP); modo 0:
// escolta [+0x50] (guarda vira STOP).
void forgetUnit(int id, uint32_t mask, int mode) {
    for (int p = 0; p < TethysGame::noPlayers() && p < kMaxPlayers; ++p) {
        if (!(mask & (1u << p))) continue;
        for (int pass = 0; pass < 2; ++pass) {
            const std::vector<int>& L = pass ? colonyVehicles(p) : colonyStructures(p);
            for (int a : L) {
                UnitRecord& r = g_units[a];
                if (!(r.flags & 0x90u) || r.id == -1 || (r.flags & UNIT_DEAD)) continue;
                if (mode) {
                    if (r.targetIsUnit() && r.hasTarget() && r.target4C == id) { dropRef(id); r.field4E = 0x7FFF; }
                    if ((r.orderTarget() & 0x80000000u) && r.orderLo == id) {
                        dropRef(id);
                        r.field4A = 0x7FFF;
                        if (r.action == 0x24) { r.action = 5; r.field20 = 1; r.field3C = 0; }
                    }
                }
                if (mode != 1 && (r.guardTarget() & 0x80000000u) && r.guardLo == id) {
                    dropRef(id);
                    r.guardHi = 0x7FFF;
                    if (r.action == 0x25) { r.action = 5; r.field20 = 1; r.field3C = 0; }
                }
            }
        }
    }
}

// sub_44D2E0: tomada (o alvo precisa estar sob EMP e ao lado).
bool takeOverStep(int id) {
    UnitRecord& u = g_units[id];
    if (!(u.orderTarget() & 0x80000000u)) return false;
    const int ti = u.orderLo;
    if (!alive(ti)) return false;
    UnitRecord& t = g_units[ti];
    if (!(t.flags & UNIT_EMP)) return false;
    if (!adjacent(u, t)) return false;
    const int old = ownerOf(t);
    if (old == ownerOf(u)) return false;
    t.flags &= ~UNIT_EMP;
    t.flags |= UNIT_LIGHTS;
    t.empTicks = 0;
    t.field22 = 0;
    vehicleTransfer(ti, ownerOf(u));                           // vt[0x68]
    const int me = TethysGame::localPlayer();
    if (old == me) postMessage(unitOX(u), unitOY(u), "Vehicle Lost", 0xA8);
    else if (ownerOf(g_units[ti]) == me) postMessage(unitOX(u), unitOY(u), "Vehicle Captured", 0xA9);
    forgetUnit(ti, g_playerData[ownerOf(u)].allyMask, 1);
    forgetUnit(ti, g_playerData[old].allyMask, 0);
    return false;
}

// sub_410F80: o ConVec começa a desmontar o prédio (ação 0x22) e espera.
void dismantleStart(int id) {
    UnitRecord& u = g_units[id];
    if (!(u.orderTarget() & 0x80000000u)) return;
    const int bi = u.orderLo;
    if (!valid(bi)) return;
    UnitRecord& b = g_units[bi];
    b.field3C = 0; b.field22 = 0;                              // vt[0x84] sub_4837C0
    b.action = 0x22; b.field20 = 1; b.field3C = 0;
    const int bt = blockInt(b.type, statOwner(b), 0x14);       // Build Time
    const int rate = blockInt(mapConVec, ownerOf(u), 0x2C);
    const int v = bt * rate;
    b.buildTime = (v + ((v >> 31) & 0x7F)) >> 7;               // [+0x5C]
    b.builderId = id;                                          // [+0x48]
    colonyMarkDirty(ownerOf(b));
    u.field3C = b.buildTime >> 2;
    u.flags &= ~UNIT_DAMAGEABLE;
    u.field22 = 0x0A;
}

} // namespace

void vehicleTransfer(int id, int player) {                    // vt[0x68] sub_44CB60
    UnitRecord& r = g_units[id];
    if (r.flags & UNIT_ARMED) unitResetCombat(id);
    if (r.action == 31 || r.action == 36 || r.action == 37 || r.action == 39) {
        r.action = 0; r.field20 = 1; r.field3C = 0;
    }
    const int old = r.owner & 0x0F;
    if (r.flags & UNIT_VEHICLE) colonyVehicleRemoved(id);
    r.owner = static_cast<uint8_t>((r.owner & 0xF0) | (player & 0x0F));
    if (r.flags & UNIT_VEHICLE) colonyVehicleAdded(id);
    colonyMarkDirty(old);
    colonyMarkDirty(player);
    aiUnitOwnerChanged(id, player);                          // sub_403080
}

void processRepairPacket(int owner, int type, const std::vector<int>& ids,
                         const std::vector<uint32_t>& wps, uint32_t tgt) {
    for (int id : ids) {
        if (!valid(id)) continue;
        UnitRecord& u = g_units[id];
        if (ownerOf(u) != (owner & 0x0F) || !commandable(u)) continue;
        orderRepair(id, type, wps, tgt);
    }
}

void repairWait8(int id) {                                     // sub_43DE50 case 8
    UnitRecord& u = g_units[id];
    const int left = --u.field3C;
    const int t = u.orderLo;
    const bool tAlive = alive(t);
    if (left != 0 && tAlive && (g_units[t].flags & UNIT_EMP)) return;
    u.field23 = u.field22;
    u.field22 = 0;
}

void repairTick(int id) {                                      // jpt_43B312 casos 28-30
    UnitRecord& u = g_units[id];
    const int act = u.action;
    const int me = TethysGame::localPlayer();
    if (u.field23 == 1) {                                      // chegou
        const int ti = u.orderLo;
        auto drop = [&](bool clearSub) {
            dropRef(ti);
            u.field4A = 0x7FFF;
            u.action = 0;
            if (clearSub) u.field22 = 0;
        };
        if (valid(ti) && (g_units[ti].flags & UNIT_BUILDING) &&
            (g_units[ti].action == 0x21 || g_units[ti].action == 0x22 || (g_units[ti].flags & 0x40000u))) {
            drop(false);
            return;
        }
        if (!alive(ti)) { drop(true); return; }
        UnitRecord& t = g_units[ti];
        if (!faceTarget(u, t)) return;
        if (act == 0x1D && (t.flags & UNIT_EMP) && t.field22 == 1) return;
        u.field22 = SUB_WAIT;
        if (act == 0x1C) {
            if (g_playerData[ownerOf(t)].ore == 0) {
                drop(true);
                if (ownerOf(u) == me) postMessage(unitOX(u), unitOY(u), "Repairs incomplete", 0xAA);
                return;
            }
            u.field3C = rateWait(u);
            soundPlayAt(unitOX(u), unitOY(u), 0x44);
        } else if (act == 0x1E) {
            u.field3C = 1;                                     // loc_43DCDF
        } else {
            u.field22 = 8;
            u.field3C = g_playerData[ownerOf(u)].isHuman ? 0x73 : 0x5F;
            soundPlayAt(unitOX(u), unitOY(u), 0x43);
        }
        return;
    }
    if (u.field23 == 0x0A) {                                   // desmonte terminou
        u.flags |= UNIT_DAMAGEABLE;
        u.action = 0;
        return;
    }
    bool more = false;
    if (act == 0x1C) more = repairStep(id);
    else if (act == 0x1D) more = takeOverStep(id);
    else {
        dismantleStart(id);
        soundPlayAt(unitOX(g_units[id]), unitOY(g_units[id]), 3);
    }
    UnitRecord& v = g_units[id];
    if (more) {
        v.field22 = SUB_WAIT;
        v.field3C = rateWait(v);
        return;
    }
    if (v.field4A != 0x7FFF) {
        const int ti = v.orderLo;
        dropRef(ti);
        v.field4A = 0x7FFF;
        if (v.action == 0x1C && ownerOf(v) == me && valid(ti)) {
            const UnitRecord& t = g_units[ti];
            const char* msg = "Repairs incomplete";
            int snd = 0xAA;
            if (t.flags & UNIT_VEHICLE) {
                if (!((blockInt(t.type, statOwner(t), 0) / 2) < t.damage)) { msg = "Vehicle Repaired"; snd = 0x6B; }
            } else if (t.damage == 0) {
                msg = "Structure Repaired"; snd = 0x6C;
            }
            postMessage(unitOX(v), unitOY(v), msg, snd);
        }
    }
    if (v.action != 0x1E) { v.action = 0; v.field22 = 0; }
}

} // namespace op2
