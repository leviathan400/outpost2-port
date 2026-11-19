#include "production_queue.h"
#include "../net/net_game.h"
#include "salvage.h"
#include "commands.h"

#include "command_apply.h"
#include "repair.h"
#include "../audio/sound.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "building_sheet.h"
#include "colony.h"
#include "construction.h"
#include "mining.h"
#include "earthwork.h"
#include "pathfinder.h"
#include "docking.h"
#include "command.h"
#include "game_map.h"
#include "player.h"
#include "research.h"
#include "scenario.h"
#include "structure.h"
#include "tethys_game.h"
#include "unit.h"
#include "weapon.h"

namespace op2 {

const std::vector<int>& uiCommandsSingle() {                 // off_4E2C30
    static const std::vector<int> k = {
        UC_MOVE, UC_STOP, UC_BUILD_KIT, UC_BUILD_MINE, UC_WALL, UC_LAVA_WALL, UC_MICROBE_WALL, UC_TUBE,
        UC_REMOVE_WALL, UC_BULLDOZE, UC_ATTACK, UC_ORE_ROUTE, UC_STAND_GROUND, UC_GUARD, UC_PATROL,
        UC_SELF_DESTRUCT, UC_DUMP_CARGO, UC_REPAIR, UC_TAKE_OVER, UC_DISMANTLE, UC_TRANSFER, UC_SALVAGE,
    };
    return k;
}
const std::vector<int>& uiCommandsMulti() {                  // off_4E2C88
    static const std::vector<int> k = {
        UC_MOVE, UC_STOP, UC_ATTACK, UC_STAND_GROUND, UC_GUARD, UC_PATROL, UC_SELF_DESTRUCT, UC_SCATTER,
        UC_TRANSFER, UC_ORE_ROUTE, UC_REPAIR,
    };
    return k;
}

const std::vector<int>& uiCommandsStructure() {              // off_4E2D38 (+ sub_4581C0)
    static const std::vector<int> k = {
        UC_IDLE, UC_ACTIVATE, UC_ATTACK, UC_BUILD, UC_ASSIGN_SCIENTISTS, UC_STOP_RESEARCH,
        UC_STOP_TRAINING, UC_ASSIGN_STUDENTS, UC_STOP, UC_STORAGE, UC_DETONATE, UC_LAUNCH_PAD,
    };
    return k;
}

static const UnitRecord* first(const std::vector<int>& sel) {
    if (sel.empty() || sel[0] <= 0 || sel[0] >= static_cast<int>(g_units.size())) return nullptr;
    return &g_units[sel[0]];
}
static int typeOf(int id) { return g_units[id].type; }
static bool anyFlag(const std::vector<int>& sel, uint32_t f) {
    for (int id : sel) if (g_units[id].flags & f) return true;
    return false;
}

// Tipo do muro de cada instância ([this+4]): concreto 0x12, lava 0x13,
// micróbio 0x14, tubo 0x11.
static int wallTypeOf(int cmd) {
    switch (cmd) {
        case UC_WALL: return 0x12;
        case UC_LAVA_WALL: return 0x13;
        case UC_MICROBE_WALL: return 0x14;
        default: return 0x11;
    }
}

bool uiCommandAvailable(int cmd, const std::vector<int>& sel) {
    const int n = static_cast<int>(sel.size());
    const UnitRecord* u = first(sel);
    const int me = TethysGame::localPlayer();
    switch (cmd) {
        case UC_MOVE:                                       // loc_4511D0
            return n && (u->flags & 0x42u);
        case UC_STOP:                                       // loc_455420
            if (n > 1) return true;
            if (!n) return false;
            if (u->type >= 1 && u->type <= 0x0F) return true;
            return u->action == 9 || u->action == 0x24;
        case UC_BUILD_KIT:                                  // loc_451A20: ConVec com kit
            return n == 1 && u->type == mapConVec && u->cargo != 0;
        case UC_BUILD_MINE: {                               // loc_451E70
            if (n != 1) return false;
            const int stats = static_cast<int8_t>(u->owner) >> 4;
            int topicType;
            if (u->type == 9) topicType = 0x15;              // Robo-Miner → mina (off_4E1394)
            else if (u->type == 0x0A) topicType = 0x2C;      // GeoCon → geotérmica (off_4E13F0)
            else return false;
            return researchHas(stats, researchTopicIndex(topicType)) || TethysGame::cheatFlag(1);
        }
        case UC_WALL: case UC_LAVA_WALL: case UC_MICROBE_WALL: case UC_TUBE: {   // sub_4524D0
            if (n != 1 || u->type != mapEarthworker) return false;
            const StructSheetDesc* d = wallSheet(wallTypeOf(cmd));
            const uint32_t own = d ? d->ownerFlags : 0;
            if (g_playerData[me].isEden ? !(own & 0x1000) : !(own & 0x800)) return false;
            return researchHas(me, researchTopicIndex(wallTypeOf(cmd)));
        }
        case UC_REMOVE_WALL:                                // loc_452AB0
            return n == 1 && u->type == mapEarthworker;
        case UC_BULLDOZE:                                   // loc_4536D0
            return n == 1 && u->type == mapRoboDozer;
        case UC_ATTACK:                                     // sub_452F90
            for (int id : sel) {
                const UnitRecord& r = g_units[id];
                if (r.flags & 0x50u) return true;
                if (r.flags & 0x80u) return structureOperational(r) && r.action != 0x24;
            }
            return false;
        case UC_ORE_ROUTE: {                                // sub_453E80: só caminhões vazios
            if (!n) return false;
            bool ok = true;
            for (int id : sel) if (typeOf(id) != mapCargoTruck || g_units[id].cargo != 0) ok = false;
            return ok;
        }
        case UC_STAND_GROUND: case UC_GUARD: case UC_PATROL:   // sub_451820 / 453410 / 454450
            return n && anyFlag(sel, UNIT_ARMED);
        case UC_SELF_DESTRUCT:                              // sub_4555A0
            return true;
        case UC_DUMP_CARGO:                                 // loc_455710
            return n == 1 && (u->type == mapCargoTruck || u->type == mapConVec) && u->cargo != 0;
        case UC_REPAIR:                                     // sub_4547E0
            if (!n) return false;
            for (int id : sel) {
                const int t = typeOf(id);
                if (t == mapConVec || t == mapRepairVehicle) return n == 1;
                if (t != mapSpider) return false;
            }
            return true;
        case UC_TAKE_OVER:                                  // loc_454BA0
            return n == 1 && u->type == mapSpider;
        case UC_DISMANTLE:                                  // loc_454F50
            return n == 1 && u->type == mapConVec;
        case UC_TRANSFER:                                   // sub_455A20
            // Trapaça dword_56DE58, ou um TRADE CENTER operando ([jogador+0x114]
            // = estatística 0x24) numa partida multijogador (sub_4027D0).
            return n && (TethysGame::cheatFlag(0) ||
                         (multiplayerMission() && g_colony[me].stat(ST_TRADE) != 0));
        case UC_SALVAGE:                                    // sub_453A20: GORF operando (+0x17C)
            return n == 1 && g_colony[me].stat(ST_GORF_GRP + 4) != 0 && u->type == mapCargoTruck && u->cargo == 0;
        case UC_SCATTER:                                    // loc_455650
            return n > 1;
        // ---- prédios ----------------------------------------------------------------
        case UC_IDLE: {                                     // sub_455BA0
            if (n != 1) return false;
            const int a = u->action;
            if (a == 0x21 || a == 0x22 || a == 0x16 || a == 9 || a == 0x17) return false;
            if (u->flags & 0x40000u) return false;
            const int t = u->type;
            if (t == 0x15 || t == 0x16 || t == 0x18 || t == 0x2A) return false;   // minas etc.
            if (t == mapCommandCenter && g_colony[u->owner & 0x0F].stat(ST_CC_OP) <= 1) return false;
            return a != 0x12;
        }
        case UC_ACTIVATE:                                   // sub_455F00
            return n == 1 && !(u->flags & 0x40000u) && u->action == 0x12;
        case UC_BUILD: {                                    // sub_456000
            if (n != 1) return false;
            const int a = u->action;
            // Fila de producao (extra do porte, 5.40): o BUILD abre a lista
            // tambem com a fabrica produzindo, para somar mais itens a fila. A
            // fila confere as regras abaixo por conta propria antes de cada item.
            if (productionQueueOn() && a == 9 && !(u->flags & 0x40000u) &&
                (u->type == 0x2D || u->type == 0x2E || u->type == 0x2F || u->type == 0x30 || u->type == 0x37))
                return true;
            if (a == 0x21 || a == 0x22 || a == 0x12 || a == 9 || (u->flags & 0x40000u)) return false;
            if (!structureOperational(*u)) return false;
            const int np = TethysGame::noPlayers() - 1 > 0 ? TethysGame::noPlayers() - 1 : 0;
            static const int kVehLimit[6] = {200, 160, 115, 90, 77, 65};   // dword_4E98D0
            static const int kBldLimit[6] = {250, 175, 100, 75, 50, 48};   // dword_4E98E8
            auto emptyBay = [&]() {                         // vt[65]: alguma baia vazia
                const StructSheetDesc* d = structSheet(u->type);
                const int bays = d ? d->player[(u->owner >> 4) & 0x0F].storageBays : 0;
                for (int i = 0; i < bays && i < 6; ++i) if (!u->bayType[i]) return true;
                return false;
            };
            switch (u->type) {
                case 0x2D: case 0x30:
                    return kVehLimit[np < 6 ? np : 5] > static_cast<int>(colonyVehicles(me).size());
                case 0x2E: return true;
                case 0x2F:
                    if (kBldLimit[np < 6 ? np : 5] <= g_colony[me].structCount) return false;
                    return emptyBay();
                case 0x37: return u->padCargo == 0 || emptyBay();
                default: return false;
            }
        }
        case UC_ASSIGN_SCIENTISTS: {                        // loc_4562C0 + sub_431FF0
            if (n != 1) return false;
            const int a = u->action;
            if (a == 0x21 || a == 0x22 || a == 0x12 || (u->flags & 0x40000u)) return false;
            if (u->type != 0x21 && u->type != 0x31 && u->type != 0x32) return false;
            return (u->flags & 0x2000u) && (u->flags & 0x4000u) && !(u->flags & 0xC0000u);
        }
        case UC_STOP_RESEARCH:                              // sub_457F50
            return n == 1 && u->action == 0x16;
        case UC_STOP_TRAINING:                              // sub_458090
            return n == 1 && u->action == 0x17;
        case UC_ASSIGN_STUDENTS: {                          // sub_456420
            if (n != 1) return false;
            const int a = u->action;
            if (a == 0x21 || a == 0x22 || a == 0x12 || (u->flags & 0x40000u) || a == 0x17) return false;
            if (!g_colony[me].workersLeft) return false;    // +0xA4 trabalhadores sobrando
            return u->type == 0x26 && structureOperational(*u);
        }
        case UC_STORAGE: {                                  // loc_456560
            if (n != 1) return false;
            if (u->action == 0x21 || u->action == 0x22 || (u->flags & 0x40000u)) return false;
            const int t = u->type;
            if (t == 0x19 || t == 0x1A) return structureOperational(*u);
            if (t == 0x27 || t == 0x29 || t == 0x2F || (t >= 0x36 && t <= 0x38)) return u->action != 0x12;
            return false;
        }
        case UC_DETONATE:                                   // loc_457DB0
            if (n != 1 || u->type != 0x17 || u->action == 0x12) return false;
            return (u->cargo == 0x43 || u->cargo == 0x44) && structureOperational(*u);
        case UC_LAUNCH_PAD:                                 // loc_4566C0
            return n == 1 && u->type == 0x37 && structureOperational(*u) && u->padCargo != 0;
        default: return false;
    }
}

static UiCommandDesc iconDesc(int icon, int slot, const char* tip) {
    UiCommandDesc d;
    d.iconRaw = icon; d.slot = slot; d.tip = tip;
    return d;
}
static UiCommandDesc textDesc(const char* label, const char* tip) {
    UiCommandDesc d;
    d.label = label; d.tip = tip;
    return d;
}

UiCommandDesc uiCommandDescribe(int cmd, const std::vector<int>& sel) {
    const UnitRecord* u = first(sel);
    const bool vehicleLike = u && (u->flags & 0x42u);
    const int me = TethysGame::localPlayer();
    switch (cmd) {
        case UC_MOVE: return iconDesc(0x48F, 1, "&Move the vehicle");
        case UC_STOP:                                       // sub_4554B0
            return vehicleLike ? iconDesc(0x48C, 2, "St&op") : textDesc("ST&OP", "St&op");
        case UC_BUILD_KIT: return iconDesc(0x47F, 4, "&Build structure kit");
        case UC_BUILD_MINE:                                 // sub_451F50
            return iconDesc(0x47F, 3, u && u->type == 9 ? "Build mi&ne or magma well" : "Build g&eothermal power plant");
        case UC_WALL: case UC_LAVA_WALL: case UC_MICROBE_WALL: case UC_TUBE: {   // sub_452590..sub_4529F0
            static const char* const kTip[4] = {"Build Concrete &Wall", "Build L&ava Wall", "Build Micro&be Wall", "Build T&ube"};
            const int k = cmd == UC_WALL ? 0 : cmd == UC_LAVA_WALL ? 1 : cmd == UC_MICROBE_WALL ? 2 : 3;
            const int icon = k == 0 ? 0x490 : k == 3 ? 0x48E : 0x48A;
            const int slot = k == 0 ? 4 : k == 3 ? 3 : 5;
            const uint8_t* blk = unitStatBlock(wallTypeOf(cmd), me);
            int common = 0, rare = 0;
            if (blk) { std::memcpy(&common, blk + 0x0C, 4); std::memcpy(&rare, blk + 0x10, 4); }
            char buf[0x32];
            if (rare) std::snprintf(buf, sizeof(buf), "%s: %i %s, %i %s", kTip[k], common, "Common", rare, "Rare");
            else std::snprintf(buf, sizeof(buf), "%s: %i %s", kTip[k], common, "Common");
            return iconDesc(icon, slot, buf);
        }
        case UC_REMOVE_WALL: return iconDesc(0x482, 6, "Remo&ve Wall/Tube");
        case UC_BULLDOZE: return iconDesc(0x483, 3, "Bulldo&ze region");
        case UC_ATTACK:                                     // sub_453040: Guard Post = texto
            if (u && u->type == 0x17) return textDesc("&ATTACK", "&Attack a unit or location");
            return iconDesc(0x47E, 4, "&Attack a unit or location");
        case UC_ORE_ROUTE: return iconDesc(0x486, 3, "S&et an ore-hauling route");
        case UC_STAND_GROUND: return iconDesc(0x48B, 6, "Sta&nd ground");
        case UC_GUARD: return iconDesc(0x481, 5, "G&uard a structure or vehicle");
        case UC_PATROL: return iconDesc(0x486, 3, "S&et a route to patrol");
        case UC_SELF_DESTRUCT: return iconDesc(0x489, 9, "Self &Destruct");
        case UC_DUMP_CARGO: return iconDesc(0x484, 6, "D&ump vehicle's cargo");
        case UC_REPAIR: return iconDesc(0x488, 3, "R&epair a structure or vehicle");
        case UC_TAKE_OVER: return iconDesc(0x487, 4, "Ta&ke over a vehicle by reprogramming");
        case UC_DISMANTLE: return iconDesc(0x482, 5, "Disma&ntle/Recycle a structure");
        case UC_TRANSFER:                                   // sub_455AA0
            return vehicleLike ? iconDesc(0x48D, 8, "Transfer to another player (&X)")
                               : textDesc("&TRANSFER", "Transfer to another player (&X)");
        case UC_SALVAGE: return iconDesc(0x618, 4, "Sal&vage rubble in a region");
        case UC_SCATTER: return iconDesc(0x480, 7, "Sca&tter");
        case UC_IDLE: return textDesc("&IDLE", "&Idle this structure");
        case UC_ACTIVATE: return textDesc("&ACTIVATE", "&Activate the structure");
        case UC_BUILD: return textDesc("&BUILD", "Select Something to &Build");
        case UC_ASSIGN_SCIENTISTS: return textDesc("&ASSIGN", "&Assign Scientists");
        case UC_ASSIGN_STUDENTS: return textDesc("&ASSIGN", "&Assign Students");
        case UC_STOP_RESEARCH: case UC_STOP_TRAINING: return textDesc("ST&OP", "St&op");
        case UC_STORAGE: return textDesc("&STORAGE", "View &Storage");
        case UC_DETONATE: return textDesc("&DETONATE", "&Detonate this tower");
        case UC_LAUNCH_PAD: return textDesc("L&AUNCH PAD", "View L&aunch Pad");
        default: return UiCommandDesc{};
    }
}

char uiCommandHotkey(const UiCommandDesc& d) {
    const std::string& s = d.tip;
    for (size_t i = 0; i + 1 < s.size(); ++i)
        if (s[i] == '&') return static_cast<char>(std::toupper(static_cast<unsigned char>(s[i + 1])));
    return 0;
}

// vt[29] dos veículos (sub_44B620): viva, sem 0x80000 (EMP) nem 0x20000, sub-estado != 0x0A.
static bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}

void unitResetCombat(int id) {                               // sub_43A910
    UnitRecord& u = g_units[id];
    auto drop = [](int ref) {
        if (ref > 0 && ref < static_cast<int>(g_units.size()) && g_units[ref].refCount) --g_units[ref].refCount;
    };
    if (u.flags & UNIT_ARMED) {
        if (u.target() & 0x80000000u) drop(u.target4C);     // [+0x4C]
        if (u.guardTarget() & 0x80000000u) drop(u.guardLo); // [+0x50]
        u.flags &= ~(0x400000u | 0x2000u);
        u.field4E = 0x7FFF;
        u.guardHi = 0x7FFF;
    }
    if (u.lastAttacker) { drop(u.lastAttacker); u.lastAttacker = 0; }
    if (u.orderTarget() & 0x80000000u) drop(u.orderLo);     // [+0x48]
    u.field4A = 0x7FFF;
}

void unitFinishStop(int id) {                                // sub_43ACA0 case 5
    UnitRecord& u = g_units[id];
    u.action = 0;
    u.field22 = 0;
    if (u.flags & UNIT_ARMED) {
        if (u.field4E != 0x7FFF) {
            if (u.targetIsUnit() && u.target4C > 0 && u.target4C < static_cast<int>(g_units.size()) &&
                g_units[u.target4C].refCount)
                --g_units[u.target4C].refCount;
            u.field4E = 0x7FFF;
        }
        if (u.field4A != 0x7FFF) {
            if ((u.orderTarget() & 0x80000000u) && u.orderLo > 0 && u.orderLo < static_cast<int>(g_units.size()) &&
                g_units[u.orderLo].refCount)
                --g_units[u.orderLo].refCount;
            u.field4A = 0x7FFF;
        }
    }
    // (prédio: vt[48] — sub-estado/produção — entra com os botões de prédio.)
    u.hasOrder = false;
    u.order = OrderBlock{};
    u.clearRoute();
    u.routeIdx = 0;
    u.field20 = 0;
}

// Pacote com a lista dos selecionados (sub_41A7E0) despachado pelo comando do
// jogador local (sub_40E210). Casos portados aqui: 5 parar, 0x14 autodestruir.
void unitPatrolTick(int id) {                                // sub_43ACA0 case 40
    UnitRecord& u = g_units[id];
    if (!u.hasOrder || u.order.waypoints.empty()) { u.action = 0; return; }
    OrderBlock& ob = u.order;
    // [ordem+4] == [ordem+8] (no último ponto) → [ordem+8] = [+0x5E].
    if (ob.countMinus1 == ob.curWaypoint) ob.curWaypoint = u.patrolStart;
    if (ob.curWaypoint < 0 || ob.curWaypoint > ob.countMinus1) ob.curWaypoint = 0;
    // sub_44C2F0 (novo objetivo) e [ordem+0x34] &= ~1; [+0x23] = [+0x22], [+0x22] = 1.
    // (No port o objetivo/rota é refeito pelo stepUnit; [+0x20] = 1 mantém o
    // movimento ligado, como na partida da ordem.)
    ob.flags &= ~1u;
    u.clearRoute();
    u.field23 = u.field22;
    u.field22 = 1;
    u.field20 = 1;
}

// A conversa daqui para baixo segue sempre o mesmo par: a funcao "send*" roda
// no CLIQUE -- valida, toca o som e MANDA o pacote --, e a "apply*" roda quando
// o pacote volta pelo turno, em todas as maquinas. Era assim no original (todo
// comando passa pelo CommandPacket) e e o que mantem a partida em rede no
// mesmo passo.
static void submit(int type, const std::vector<int>& ids, uint32_t target = 0,
                   const std::vector<uint32_t>* wps = nullptr) {
    CommandPacket pkt;
    pkt.type = type;
    pkt.target = target;
    if (wps) pkt.waypoints = *wps;
    const int me = TethysGame::localPlayer();
    for (int id : ids)
        if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == me)
            pkt.unitIds.push_back(id);
    if (!pkt.unitIds.empty()) commandSubmit(me, pkt);
}

static void sendStop(const std::vector<int>& sel) {         // sub_455540: pacote 5
    submit(CMD_STOP, sel);
}
void applyStop(int owner, const std::vector<int>& ids) {    // case 5
    for (int id : ids) {
        if (id <= 0 || id >= static_cast<int>(g_units.size())) continue;
        UnitRecord& r = g_units[id];
        if ((r.owner & 0x0F) != (owner & 0x0F) || !commandable(r)) continue;
        r.action = 5;
        r.field20 = 1;
        r.field3C = 0;
        if (r.flags & UNIT_ARMED) unitResetCombat(id);
    }
}
static void sendSelfDestruct(const std::vector<int>& sel) { // sub_4555F0: pacote 0x14
    submit(CMD_SELF_DESTRUCT, sel);
}
void applySelfDestruct(int owner, const std::vector<int>& ids) {   // case 20
    for (int id : ids)
        if (id > 0 && id < static_cast<int>(g_units.size()) &&
            (g_units[id].owner & 0x0F) == (owner & 0x0F))
            unitDoSelfDestruct(id);
}

// DETONATE (sub_457E40): pacote 0x24 (atacar) com as unidades da seleção que
// têm 0x80 (prédio-torre), sem pontos, alvo = o tile do 1º selecionado; som 0x56.
static void sendDetonate(const std::vector<int>& sel) {
    if (sel.empty()) return;
    const int me = TethysGame::localPlayer();
    std::vector<int> ids;
    for (int id : sel)
        if ((g_units[id].owner & 0x0F) == me && (g_units[id].flags & UNIT_TURRET_BLD)) ids.push_back(id);
    const UnitRecord& b = g_units[sel[0]];
    const uint32_t tgt = static_cast<uint32_t>((unitOX(b) >> 5) & 0xFFFF) | (static_cast<uint32_t>(unitOY(b) >> 5) << 16);
    if (!ids.empty()) submit(CMD_ATTACK, ids, tgt);
    soundPlay(0x56);
}

// sub_4241D0 (pacote 0x15): centro das unidades vivas (x com a volta, a partir
// da 1ª) e cada uma vai p/ o ponto espelhado: pos + (pos − centro), y preso a
// [0, (H−1)*32]. Ordem de mover (ação 2) com 1 ponto, [ordem+0x34] &= ~1.
static void sendScatter(const std::vector<int>& sel) {       // sub_4556B0: pacote 0x15
    submit(CMD_SCATTER, sel);
}
void applyScatter(int owner, const std::vector<int>& sel) {
    (void)owner;
    const int off = g_gameMap.internalXOffset() * 32;
    const int pxW = g_gameMap.internalWidth() * 32, pxMask = pxW - 1;
    auto wrap = [&](int d) {                                 // padrão com dword_54EBBC
        const int h = pxMask / 2;
        if (-h >= d) return d + pxW;
        if (h < d) return -((-d) & pxMask);
        return d;
    };
    auto alive = [](int id) { return g_units[id].id != -1 && !(g_units[id].flags & UNIT_DEAD); };
    size_t i0 = 0;
    while (i0 < sel.size() && !alive(sel[i0])) ++i0;
    if (i0 == sel.size()) return;
    const int x0 = unitOX(g_units[sel[i0]]) + off;
    int dxSum = 0, ySum = unitOY(g_units[sel[i0]]), n = 1;
    for (size_t i = i0 + 1; i < sel.size(); ++i) {
        if (!alive(sel[i])) continue;
        dxSum += wrap(unitOX(g_units[sel[i]]) + off - x0);
        ySum += unitOY(g_units[sel[i]]);
        ++n;
    }
    const int cx = (x0 + dxSum / n) & pxMask, cy = ySum / n;
    const int H = g_gameMap.height();
    for (int id : sel) {
        if (!alive(id)) continue;
        UnitRecord& u = g_units[id];
        const int ux = unitOX(u) + off, uy = unitOY(u);
        const int dx = wrap(ux - cx);
        const int tx = (ux + dx) & pxMask;
        int ty = uy + (uy - cy);
        if (ty < 0) ty = 0;
        else if (H * 32 <= ty) ty = (H - 1) * 32;
        const int px = tx - off;
        u.action = 2; u.field20 = 1; u.field3C = 0;
        u.hasOrder = true;
        u.order = OrderBlock{};
        u.order.count = 1;
        u.order.waypoints.push_back(packLocation((px >> 5) & (g_gameMap.width() - 1), ty >> 5));
        u.clearRoute();
    }
}

// Patrulhar (off_4D4D60, instância dword_564940): [+4] já tem o 1º ponto,
// [+0xC] nº de pontos, [+0xE] índice do ponto inicial, [+0x12...] pontos.
// (A fila de pontos unk_5471B0/dword_5471AC do original é CÓDIGO MORTO, o que
// foi conferido em 2026-10-29: o contador é o campo +0x3C do objeto do modo de
// mira (dword_547170), sub_41B550 o ZERA ao entrar no modo, as três funções
// que o leem — sub_451420, sub_453FA0 e sub_454540 — só leem, e não há uma
// única escrita em +0x3C em toda a região do modo de mira. Ou seja, o jogo
// publicado nunca enfileira ponto nenhum: todo comando sai com UM ponto, o do
// clique. A "fila vazia" do port é o comportamento do original, não uma
// pendência.)
static struct {
    int stage = 0;                  // [+4]
    std::vector<uint32_t> pts;      // [+0x12...]
    int start = 0;                  // [+0xE]
} s_patrol;
static std::string s_hint;
// Rota de minério (objeto do comando off_4D4D38): [+4] mina, [+8] smelter,
// [+0x10] nº de pontos, [+0x12] índice do ponto da mina, [+0x14] do smelter.
static struct {
    int mine = 0, smelter = 0;
    std::vector<uint32_t> pts;
    int mineIdx = 0, smelterIdx = 0;
} s_ore;

const std::string& uiCommandHint() { return s_hint; }

namespace {
int s_wpCap = 0;                                    // [dword_547170+0x34]
std::vector<WaypointPx> s_wp;                       // [+0x3C] / [+0x40..]
}
void uiWaypointsArm(int cap) { s_wpCap = cap < 0 ? 0 : cap; s_wp.clear(); }
int uiWaypointCap() { return s_wpCap; }
const std::vector<WaypointPx>& uiWaypoints() { return s_wp; }
bool uiWaypointAdd(int px, int py) {                // sub_41AAA0, WM_LBUTTONDOWN com MK_CONTROL
    if (s_wpCap <= 0) return false;                 // [+0x34] == 0: nada
    if (static_cast<int>(s_wp.size()) >= s_wpCap) s_wp.pop_back();   // cheio: dec [+0x3C]
    s_wp.push_back({px, py});
    soundPlay(0x59);
    return true;
}

// sub_451420: cada ponto da mira vira um LOCATION de pixel -- x nos bits 0..14,
// y nos 15..28 -- com a folga de chegada nos bits 29..31 (dl = 2: chegar a ate
// 2 tiles). Vem ANTES do alvo do comando.
static void appendWaypoints(std::vector<uint32_t>& out) {
    for (const WaypointPx& w : s_wp) {
        uint32_t v = static_cast<uint32_t>(w.x) & 0x7FFFu;
        v |= (static_cast<uint32_t>(w.y) << 15) & 0x1FFF8000u;
        v |= 2u << 29;
        out.push_back(v);
    }
}

// O limite de pontos de cada comando, o 3o argumento do sub_41B550 em cada
// vt[1]: 7 em quase todos; atacar (sub_4530D0) e o sub_4552C0 caem para 0 quando
// o unico selecionado e um predio armado (bit 0x80), que nao anda; patrulha 6
// (sub_4544F0), rota de minerio 5 (sub_453F50); muros, terraplenagem e salvage
// de regiao, 0. Remover muro (sub_452B40) aceita 7 no original, mas aqui o
// Earthworker vai ate o muro sem lista de pontos (applyRemoveWall): fica 0.
static int waypointCapFor(int cmd, const std::vector<int>& sel) {
    switch (cmd) {
        case UC_MOVE: case UC_STAND_GROUND: case UC_GUARD:
        case UC_BUILD_KIT: case UC_BUILD_MINE:
        case UC_REPAIR: case UC_TAKE_OVER: case UC_DISMANTLE:
            return 7;
        case UC_ATTACK:
            if (sel.size() == 1 && (g_units[sel[0]].flags & 0x80u)) return 0;
            return 7;
        case UC_PATROL: return 6;
        case UC_ORE_ROUTE: return 5;
        default: return 0;
    }
}

// Ponto da patrulha: pixel do clique empacotado (x & 0x7FFF) | (y << 15), com
// os bits 29-31 = 3 (chegar a até 3 tiles de cada ponto intermediário).
static uint32_t patrolPoint(int tileX, int tileY) {
    return packLocation(tileX, tileY) | 0x60000000u;
}

static UiClick uiCommandClickImpl(int cmd, const std::vector<int>& sel);

UiClick uiCommandClick(int cmd, const std::vector<int>& sel) {
    // sub_41B550(estado, comando, limite, ...): armar a mira poe o ESTADO
    // ([+0x24]: 1/4 area, 2 posicionamento, 3 ponto), o limite de pontos de
    // passagem ([+0x34]) e zera a contagem ([+0x3C]). Os comentarios da tabela
    // abaixo dizem os dois de cada comando.
    const UiClick r = uiCommandClickImpl(cmd, sel);
    uiWaypointsArm(r == UiClick::TARGET ? waypointCapFor(cmd, sel) : 0);
    return r;
}

static UiClick uiCommandClickImpl(int cmd, const std::vector<int>& sel) {
    s_hint.clear();
    switch (cmd) {
        case UC_STOP:                                                    // sub_455540 (pacote 5)
            // Fila de producao (5.40): parar a fabrica para tambem o resto da fila.
            if (productionQueueOn()) for (int id : sel) productionQueueClear(id);
            sendStop(sel);
            return UiClick::DONE;
        case UC_SELF_DESTRUCT: sendSelfDestruct(sel); return UiClick::DONE;   // sub_4555F0 (0x14)
        case UC_MOVE: return UiClick::TARGET;                           // sub_451260 (estado 3, ate 7 pontos)
        case UC_STAND_GROUND: return UiClick::TARGET;                   // sub_4518E0 (estado 3, ate 7 pontos)
        case UC_ATTACK: return UiClick::TARGET;                         // sub_4530D0
        case UC_GUARD: return UiClick::TARGET;                          // sub_4534D0 (estado 3, ate 7 pontos)
        case UC_BUILD_KIT: return UiClick::TARGET;                      // sub_451AC0 (estado 2, área do kit; ate 7 pontos)
        case UC_BUILD_MINE: return UiClick::TARGET;                     // sub_451FC0 (estado 2, área da mina; ate 7 pontos)
        case UC_WALL: case UC_LAVA_WALL: case UC_MICROBE_WALL: case UC_TUBE:   // sub_452640 (estado 4, sem pontos)
        case UC_BULLDOZE:                                               // sub_453760 (estado 1, sem pontos)
        case UC_REMOVE_WALL:                                            // sub_452B40 (estado 3, ate 7 pontos no original)
            return UiClick::TARGET;
        case UC_ORE_ROUTE:                                              // sub_453F50 (estado 3, ate 5 pontos)
            s_ore = {};
            s_hint = "Set path to mine";
            return UiClick::TARGET;
        case UC_PATROL:                                                 // sub_4544F0 (estado 3, ate 6 pontos)
            s_patrol = {};
            s_hint = "Set path to initial patrol point";
            return UiClick::TARGET;
        case UC_SCATTER: sendScatter(sel); return UiClick::DONE;        // sub_4556B0 (0x15)
        case UC_SALVAGE: return UiClick::TARGET;                        // sub_453B20 (estado 3, 1 ponto; aqui, região)
        case UC_DETONATE: sendDetonate(sel); return UiClick::DONE;      // sub_457E40 (0x24 no próprio tile)
        case UC_REPAIR: case UC_TAKE_OVER: case UC_DISMANTLE:          // sub_4548B0 etc. (estado 3, ate 7 pontos)
            return UiClick::TARGET;
        // IDLE (pacote 0x12) / ACTIVATE (0x13). Quando desligar o prédio faria
        // o metal passar do teto de armazenagem, quem pergunta antes é o
        // GameState (ConfirmPane + structureIdleMetalLoss), como sub_455CB0.
        // TRANSFER (sub_455A80): o vt[3] e nullsub_84 porque o comando NAO pede
        // alvo no mapa -- ele abre o ESCOLHEDOR DE JOGADOR ("Transfer
        // Vehicle(s) to:", sub_46D230), e e a escolha que monta o pacote 0x18
        // com as unidades e o destino (sub_455950). Quem abre o escolhedor e o
        // GameState, que e quem tem painel.
        case UC_TRANSFER: return UiClick::CHOOSE_PLAYER;
        case UC_IDLE: if (!sel.empty()) submit(CMD_IDLE, {sel[0]}); return UiClick::DONE;
        case UC_ACTIVATE: if (!sel.empty()) submit(CMD_UNIDLE, {sel[0]}); return UiClick::DONE;
        default: return UiClick::NOT_PORTED;
    }
}

bool uiCommandIsLine(int cmd) {
    return cmd == UC_WALL || cmd == UC_LAVA_WALL || cmd == UC_MICROBE_WALL || cmd == UC_TUBE;
}
bool uiCommandIsArea(int cmd) { return uiCommandIsLine(cmd) || cmd == UC_BULLDOZE || cmd == UC_SALVAGE; }

UiClick uiCommandArea(int cmd, const std::vector<int>& sel, int x1, int y1, int x2, int y2) {
    if (cmd == UC_SALVAGE) { sendSalvage(sel, x1, y1, x2, y2); soundPlay(0x56); return UiClick::DONE; }
    const bool ok = cmd == UC_BULLDOZE ? sendBulldoze(sel, x1, y1, x2, y2)          // sub_453780
                                       : sendBuildWall(sel, x1, y1, x2, y2, wallTypeOf(cmd));   // sub_452660
    if (!ok) { soundPlay(0x58); return UiClick::TARGET; }   // sub_41AAA0: recusado
    return UiClick::DONE;
}

bool uiCommandCellOk(int cmd, const std::vector<int>& sel, int x, int y) {
    if (!sel.empty() && sel[0] > 0 && sel[0] < static_cast<int>(g_units.size())) pfSetContext(sel[0]);
    switch (cmd) {
        case UC_BULLDOZE: return dozeCellOk(x, y);                     // sub_438E50
        case UC_SALVAGE:  return salvageCellOk(sel.empty() ? 0 : sel[0], x, y);
        default:          return wallCellOk(x, y, wallTypeOf(cmd));    // sub_438C60
    }
}

static int s_curPX = 0, s_curPY = 0;
void uiSetCursorPixel(int px, int py) { s_curPX = px; s_curPY = py; }

// Unidade sob o cursor (sub_437840/sub_437890), flags & flagMask.
static int unitUnderCursor(uint32_t flagMask) {
    MapPointIter it;
    mapPointIterInit(it, s_curPX + g_gameMap.internalXOffset() * 32, s_curPY, 0xFFFFFFFFu, flagMask);
    return mapPointIterNext(it);
}

// vt[3] de REPAIR (sub_4548D0) / TAKE OVER (sub_454C50) / DISMANTLE (sub_455000):
// alvo sob o cursor e pacote 0x1C/0x1D/0x1E com os selecionados (som 0x56).
static void sendRepairLike(int cmd, const std::vector<int>& sel) {
    const int me = TethysGame::localPlayer();
    int tgt = 0;
    if (cmd == UC_REPAIR) {
        const int t = unitUnderCursor(0xFFFFFFFFu);
        if (t) {
            const UnitRecord& r = g_units[t];
            const UnitRecord* f = sel.empty() ? nullptr : &g_units[sel[0]];
            if ((g_playerData[r.owner & 0x0F].allyMask & (1u << me)) && r.type != 3 && r.type != 4 && f &&
                f->id != t) {
                if (f->type == mapConVec) { if (r.flags & UNIT_BUILDING) tgt = t; }
                else if (f->type == mapSpider || f->type == mapRepairVehicle) tgt = t;
            }
        }
    } else if (cmd == UC_TAKE_OVER) {
        const int t = unitUnderCursor(2u);
        if (t) {
            const UnitRecord& r = g_units[t];
            if (!(g_playerData[r.owner & 0x0F].allyMask & (1u << me)) && r.type != 3 && r.type != 4) tgt = t;
        }
    } else {
        const int t = unitUnderCursor(4u);
        if (t) {
            const UnitRecord& r = g_units[t];
            if ((r.owner & 0x0F) == me && r.action != 0x22 && r.action != 0x21 && r.type != 0x15 &&
                r.type != 0x16 && r.type != 0x2A && r.type != 0x2C)
                tgt = t;
        }
    }
    const int type = cmd == UC_REPAIR ? 0x1C : cmd == UC_TAKE_OVER ? 0x1D : 0x1E;
    std::vector<uint32_t> wps;
    appendWaypoints(wps);                                    // sub_451420 (sem o alvo)
    submit(type, sel, static_cast<uint32_t>(tgt) | 0xFFFF0000u, &wps);
    soundPlay(0x56);
}

// O COMANDO PADRAO do original (dword_5648A0, vtable off_4D4E00): com unidades
// selecionadas e nenhum comando armado, sub_4552C0 junta, NESTA ORDEM, os
// comandos de off_4E2C00 disponiveis para a selecao -- atacar (dword_56484C),
// reprogramar do Spider (dword_564988) e mover (dword_56498C) -- e arma o
// estado 3. Sob o ponteiro, sub_455390 pergunta o vt[8] de cada um e o
// PRIMEIRO que aceitar da o cursor (vt[9]); o clique (sub_455350) executa
// esse. Nenhum aceitando: a seta, e o clique fica com a selecao.
//   atacar (sub_453260 -> sub_452E00): qualquer unidade sob o ponteiro cujo
//     dono NAO tem o jogador local como aliado; com um Guard Post
//     selecionado, so dentro do alcance (bloco + 0x18, distancia em tiles
//     max + min/2 com a volta do mapa);
//   reprogramar (sub_454E70; disponivel so com UM Spider selecionado,
//     loc_454BA0): veiculo nao aliado, que nao seja Spider nem Scorpion, e
//     paralisado pelo EMP ([+0x44] & 0x80000);
//   mover (sub_451640): sobre um PREDIO do jogador local, so a doca dele,
//     livre e que o veiculo possa usar (vira atracar, sub_44DA80); fora de
//     predio, aceita se nao houver unidade do jogador local sob o ponteiro.
int uiDefaultCommand(const std::vector<int>& selIn, int tileX, int tileY) {
    // O cursor pergunta a CADA quadro, com a selecao que estiver na tela: so
    // entram ids vivos e dentro da tabela (uma selecao velha -- unidades
    // recriadas, jogo carregado -- derrubava o uiCommandAvailable, 5.42).
    std::vector<int> sel;
    for (int id : selIn)
        if (id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1 &&
            !(g_units[id].flags & UNIT_DEAD))
            sel.push_back(id);
    if (sel.empty() || !g_gameMap.ok()) return -1;
    const int me = TethysGame::localPlayer();
    auto aliadoDeMim = [&](int id) {
        const int o = g_units[id].owner & 0x0F;
        return o < kMaxPlayers && (g_playerData[o].allyMask & (1u << me)) != 0;
    };
    auto sob = [&](uint32_t playerMask, uint32_t flagMask) {
        MapPointIter it;
        mapPointIterInit(it, s_curPX + g_gameMap.internalXOffset() * 32, s_curPY, playerMask, flagMask);
        return mapPointIterNext(it);
    };
    if (uiCommandAvailable(UC_ATTACK, sel)) {                // dword_56484C
        const int t = sob(0xFFFFFFFFu, 0xFFFFFFFFu);
        if (t && !aliadoDeMim(t)) {
            bool ok = true;
            const UnitRecord& f = g_units[sel[0]];
            if (f.type == 0x17) {                            // Guard Post: o alcance
                const UnitRecord& a = g_units[t];
                const int W = g_gameMap.width() * 32;
                int dx = std::abs(f.pixelX - a.pixelX);
                if (dx > W / 2) dx = W - dx;
                const int dy = std::abs(f.pixelY - a.pixelY);
                const int d = (std::max(dx, dy) + std::min(dx, dy) / 2) >> 5;
                const uint8_t* b = unitStatBlock(f.type, f.owner & 0x0F);
                int32_t alcance = 0;
                if (b) std::memcpy(&alcance, b + 0x18, 4);
                ok = alcance >= d;
            }
            if (ok) return UC_ATTACK;
        }
    }
    if (uiCommandAvailable(UC_TAKE_OVER, sel)) {             // dword_564988
        const int t = sob(0xFFFFFFFFu, 2u);
        if (t && !aliadoDeMim(t)) {
            const UnitRecord& a = g_units[t];
            if (a.type != 3 && a.type != 4 && (a.flags & 0x80000u)) return UC_TAKE_OVER;
        }
    }
    if (uiCommandAvailable(UC_MOVE, sel)) {                  // dword_56498C
        const int b = sob(1u << me, 4u);                     // predio do jogador local
        if (b) {
            int dx = 0, dy = 0;
            const int occ = g_gameMap.cellOccupant(tileX & (g_gameMap.width() - 1), tileY);
            // sub_451640: fora da doca, recusa. NA doca: quem nao atraca ali
            // (sub_44DA80 = 0) e aceito como mover; quem atraca, so com a
            // doca livre (7FF0000 da celula).
            if (!dockCell(b, dx, dy) || dx != tileX || dy != tileY) return -1;
            if (!vehicleCanDock(sel[0], b, true)) return UC_MOVE;
            return occ <= 0 ? UC_MOVE : -1;
        }
        if (!sob(1u << me, 0xFFFFFFFFu)) return UC_MOVE;
    }
    return -1;
}

bool uiOreRouteTargetOk(const std::vector<int>& sel, int tileX, int tileY) {
    const UnitRecord* u = first(sel);
    if (!u || !g_gameMap.ok()) return false;
    const int stage = !s_ore.mine ? 0 : !s_ore.smelter ? 1 : 2;
    return oreRouteTarget(stage, s_ore.mine, u->owner & 0x0F,
                          tileX & (g_gameMap.width() - 1), tileY) != 0;
}

UiClick uiCommandTarget(int cmd, const std::vector<int>& sel, int tileX, int tileY) {
    if (cmd == UC_REPAIR || cmd == UC_TAKE_OVER || cmd == UC_DISMANTLE) { sendRepairLike(cmd, sel); return UiClick::DONE; }
    if (cmd == UC_BUILD_MINE) {                              // sub_452010
        std::vector<uint32_t> wps; appendWaypoints(wps);
        sendBuildMine(sel, tileX, tileY, wps);
        return UiClick::DONE;
    }
    if (cmd == UC_REMOVE_WALL) {                             // sub_452B60 + sub_452D10
        if (!sendRemoveWall(sel, tileX, tileY)) { soundPlay(0x58); return UiClick::TARGET; }
        return UiClick::DONE;
    }
    if (cmd == UC_ORE_ROUTE) {                               // sub_454290 + sub_453FA0
        const UnitRecord* u = first(sel);
        if (!u) return UiClick::DONE;
        const int stage = !s_ore.mine ? 0 : !s_ore.smelter ? 1 : 2;
        const int b = oreRouteTarget(stage, s_ore.mine, u->owner & 0x0F, tileX, tileY);
        if (!b) { soundPlay(0x58); return UiClick::TARGET; } // sub_41AAA0: alvo recusado
        // sub_453FA0: os pontos do Ctrl entram antes do clicado, e os indices da
        // mina e da fundicao apontam para o CLICADO ([+0x12] = contagem antes dele).
        for (const WaypointPx& w : s_wp) s_ore.pts.push_back(packLocation(w.x >> 5, w.y >> 5));
        s_ore.pts.push_back(packLocation(tileX, tileY));
        soundPlay(0x59);
        if (stage == 0) {
            s_ore.mine = b; s_ore.mineIdx = static_cast<int>(s_ore.pts.size()) - 1;
            s_hint = "Set path from mine to smelter";
            uiWaypointsArm(6 - static_cast<int>(s_ore.pts.size()));
            return UiClick::TARGET;
        }
        if (stage == 1) {
            s_ore.smelter = b;
            s_ore.smelterIdx = static_cast<int>(s_ore.pts.size()) - 1;
            s_hint = "Set path from smelter back to mine";
            uiWaypointsArm(7 - static_cast<int>(s_ore.pts.size()));
            return UiClick::TARGET;
        }
        s_hint.clear();
        sendOreRoute(sel, s_ore.pts, s_ore.mineIdx, s_ore.smelterIdx, s_ore.mine, s_ore.smelter);
        soundPlay(0x56);
        s_ore = {};
        return UiClick::DONE;
    }
    if (cmd == UC_PATROL) {                                  // sub_454540
        // sub_454540: cada clique copia antes os pontos do Ctrl ([+0x3C] de
        // dword_547170) e so entao o ponto clicado.
        for (const WaypointPx& w : s_wp) s_patrol.pts.push_back(patrolPoint(w.x >> 5, w.y >> 5));
        if (s_patrol.stage == 0) {                           // 1º clique: ponto inicial
            s_patrol.stage = 1;
            s_patrol.start = static_cast<int>(s_patrol.pts.size());   // [+0xC] = [+0xE] = fila
            s_patrol.pts.push_back(patrolPoint(tileX, tileY));
            s_hint = "Set path to final patrol point";
            soundPlay(0x59);
            uiWaypointsArm(7 - static_cast<int>(s_patrol.pts.size()));   // mira de novo (7 - n pontos)
            return UiClick::TARGET;
        }
        s_patrol.pts.push_back(patrolPoint(tileX, tileY));   // 2º clique: ponto final
        soundPlay(0x59);
        s_hint.clear();
        // Pacote 0x28: unidades selecionadas (sub_41A7E0, máscara -1), os
        // pontos e as palavras [+0xE]/[+0x10]. (Som 0x56.)
        const int me = TethysGame::localPlayer();
        CommandPacket pkt;
        pkt.type = 0x28;
        pkt.waypoints = s_patrol.pts;
        pkt.patrolStart = s_patrol.start;
        for (int id : sel)
            if ((g_units[id].owner & 0x0F) == me && (g_units[id].flags & UNIT_VEHICLE)) pkt.unitIds.push_back(id);
        if (!pkt.unitIds.empty()) commandSubmit(me, pkt);
        s_patrol = {};
        soundPlay(0x56);
        return UiClick::DONE;
    }
    if (cmd == UC_STAND_GROUND) {                            // sub_451900: pacote 0x26, armados (0x10)
        const int me = TethysGame::localPlayer();
        CommandPacket pkt;
        pkt.type = 0x26;
        appendWaypoints(pkt.waypoints);                      // sub_451420: pontos do Ctrl...
        pkt.waypoints.push_back(packLocation(tileX, tileY)); // ...e o alvo
        for (int id : sel)
            if ((g_units[id].owner & 0x0F) == me && (g_units[id].flags & UNIT_ARMED)) pkt.unitIds.push_back(id);
        if (!pkt.unitIds.empty()) commandSubmit(me, pkt);
        soundPlay(0x56);
        return UiClick::DONE;
    }
    if (cmd == UC_ATTACK) {                                  // sub_453130: pacote 0x24 (armados 0x10|0x80)
        const int me = TethysGame::localPlayer();
        std::vector<int> ids;
        for (int id : sel)
            if ((g_units[id].owner & 0x0F) == me && (g_units[id].flags & 0x90u)) ids.push_back(id);
        // Alvo = unidade sob o cursor (dword_56DDE4) ou o tile.
        uint32_t tgt = static_cast<uint32_t>(static_cast<uint16_t>(tileX)) |
                       (static_cast<uint32_t>(static_cast<uint16_t>(tileY)) << 16);
        const int occ = g_gameMap.cellOccupant(tileX & (g_gameMap.width() - 1), tileY);
        if (occ > 0 && occ < static_cast<int>(g_units.size()) && g_units[occ].id != -1 &&
            !(g_units[occ].flags & UNIT_DEAD))
            tgt = static_cast<uint32_t>(occ) | 0xFFFF0000u;
        if (!ids.empty()) {
            CommandPacket pkt;                               // pacote 0x24
            pkt.type = CMD_ATTACK;
            pkt.unitIds = ids;
            pkt.target = tgt;
            appendWaypoints(pkt.waypoints);                  // sub_451420 (sem o alvo)
            commandSubmit(me, pkt);
        }
        soundPlay(0x56);
        return UiClick::DONE;
    }
    if (cmd == UC_BUILD_KIT) {                               // sub_451B30
        std::vector<uint32_t> wps; appendWaypoints(wps);
        sendBuildKit(sel, tileX, tileY, wps);
        return UiClick::DONE;
    }
    if (cmd == UC_GUARD) {                                   // sub_4534F0: pacote 0x25
        std::vector<uint32_t> wps;
        appendWaypoints(wps);                                // sub_451420 (sem o alvo)
        sendGuardTarget(TethysGame::localPlayer(), sel, tileX, tileY, wps);
        soundPlay(0x56);
        return UiClick::DONE;
    }
    if (cmd != UC_MOVE) return UiClick::NOT_PORTED;
    // sub_451280: na célula da doca de um prédio → pacote 3 (ou 4 na garagem de
    // Eden) se o 1º selecionado pode atracar (sub_44DA80); se não pode, nada
    // no original (aqui, fork: mover simples até a doca).
    // Fora da doca → pacote 2 (mover).
    const int me = TethysGame::localPlayer();
    CommandPacket pkt;
    pkt.type = CMD_MOVE;
    {
        const int occ = buildingAtTile(tileX & (g_gameMap.width() - 1), tileY, -1);   // sub_437EB0 (classe 4)
        int dx, dy;
        if (occ > 0 && occ < static_cast<int>(g_units.size()) && (g_units[occ].flags & UNIT_BUILDING) &&
            dockCell(occ, dx, dy) && (dx & (g_gameMap.width() - 1)) == (tileX & (g_gameMap.width() - 1)) && dy == tileY) {
            // Fork: the original returns here, sending nothing, when the first
            // selected vehicle can't use this dock (e.g. a loaded truck on a
            // mine, which only takes empty ones), so the truck just ignored the
            // click. Here it drives onto the dock tile with a plain move instead.
            if (!sel.empty() && vehicleCanDock(sel[0], occ, true)) {
                const UnitRecord& b = g_units[occ];
                const bool edenBld = (b.owner & 0x0F) < kMaxPlayers && g_playerData[b.owner & 0x0F].isEden;
                pkt.type = (b.type == 0x29 && edenBld) ? 4 : 3;
            }
        }
    }
    appendWaypoints(pkt.waypoints);                          // sub_451420: os pontos do Ctrl...
    pkt.waypoints.push_back(packLocation(tileX, tileY));     // ...e o alvo (folga 0)
    for (int id : sel)
        if ((g_units[id].owner & 0x0F) == me && (g_units[id].flags & UNIT_VEHICLE)) pkt.unitIds.push_back(id);
    if (!pkt.unitIds.empty()) commandSubmit(me, pkt);
    soundPlay(0x56);
    return UiClick::DONE;
}

} // namespace op2
