#include "ai_group.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

#include "building_sheet.h"
#include "colony.h"
#include "command.h"
#include "commands.h"
#include "construction.h"
#include "docking.h"
#include "earthwork.h"
#include "game_map.h"
#include "mining.h"
#include "pathfinder.h"
#include "production.h"
#include "repair.h"
#include "structure.h"
#include "walls.h"
#include "player.h"
#include "scenario.h"
#include "tethys_game.h"
#include "trigger.h"
#include "unit.h"
#include "weapon.h"
#include "save_stream.h"

namespace op2 {

namespace {

bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
bool alive(int id) { return valid(id) && !(g_units[id].flags & UNIT_DEAD); }
int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
int tick() { return TethysGame::tick(); }

// Coordenadas INTERNAS do original (o que o grupo guarda): [+0x14]/[+0x18].
int pxW() { return g_gameMap.internalWidth() * 32; }                    // dword_54EBC0
int pxMask() { return pxW() - 1; }                                       // dword_54EBBC
void origPos(const UnitRecord& r, int& x, int& y) {
    unitOrigPixel(r, x, y);
    x += g_gameMap.internalXOffset() * 32;
}
int wrapDX(int dx) {                                                     // diferença em x com volta
    const int half = pxMask() / 2;
    if (-half >= dx) dx += pxW();
    else if (half < dx) dx = -((-dx) & pxMask());
    return dx;
}
int portTileX(int ix) {                                                  // px interno → tile do port
    return ((ix >> 5) - g_gameMap.internalXOffset()) & (g_gameMap.width() - 1);
}
bool inRect(const int r[4], int x, int y) {                              // retângulo em px (x com volta)
    if (r[1] > y || r[3] < y) return false;
    if (r[2] > r[0]) return x >= r[0] && x <= r[2];
    return x >= r[0] || x <= r[2];
}
// sub_490C50: o jogador j tem p como aliado?
bool allyOf(int j, int p) { return (g_playerData[j].allyMask & (1u << p)) != 0; }
// sub_490CD0: máscara dos jogadores j que não têm p como aliado (com os bits
// altos que a conta original deixa passar).
uint32_t enemyMask(int p) {
    uint32_t m = 0;
    const int n = TethysGame::noPlayers();
    for (int j = 0; j < n && j < kMaxPlayers; ++j)
        m |= ((g_playerData[j].allyMask ^ (1u << p)) >> p) << j;
    return m;
}

struct Node { int unit = 0; int tick = 0; int cls = 0; };

// Lista de alvos (var_54 do vt[13]): 16 unidades, +0x40 contagem, +0x44..+0x50.
struct TargetList {
    int u[16] = {};
    int count = 0;            // +0x40
    int vehicles = 0;         // +0x44
    int foamedOrPowered = 0;  // +0x48
    int unpoweredOrEmpBld = 0;// +0x4C
    int empVehicles = 0;      // +0x50
};

// TargCount: {tipo, arma (0x73 = qualquer), alvo, atual}.
struct TCEntry { int type, weapon, target, current; };
// BuildGroup: prédio/mina {x1,y1,x2,y2 (tiles internos), tipo, +0x14, +0x18}
// (prédio: carga, grupo; mina: grupo, unidade designada), tubo {x, y, tipo,
// unidade}, grupo a reforçar {stub, prioridade, fábrica}.
struct BuildRec { int rc[4]; int type; int a14; int a18; };
struct TubeRec { int x, y, type, worker; };
struct ReinfRec { int stub, prio, factory; };

struct Group : Stub {
    int kind = GK_SC;
    int player = 0;                 // +0x2F4
    int deleteAt = 0x7FFFFFFF;      // +0x2F8
    int lights = 1;                 // +0x2FC
    std::vector<Node> nodes;        // +0x2E8.. (ordem por classe)
    // FightGroup (sub_428100)
    int combine = 0, combineTarget = 0, combineTick = 0;     // +0x308/+0x30C/+0x310
    int patrolN = 0, patrolStart = 0;                        // +0x314/+0x318
    uint32_t patrolPts[8] = {};                              // +0x31C (tiles do port, 0x60000000)
    int patrolOn = 0;                                        // +0x33C
    int follow = 0;                                          // +0x340
    int rect[4] = {0, -1, 0, 0};                             // +0x344 (px internos)
    int targetGroup = -1, attackType = 0, targetUnit = -1;   // +0x354/+0x358/+0x35C
    std::vector<std::array<int, 4>> guarded;                 // +0x360/+0x364 (até 8)
    int mode = 0;                                            // +0x3E4
    // TargCount (+0x2E4, criado por sub_42B050; +0x10 = recontar).
    bool hasTC = false, tcDirty = false;
    std::vector<TCEntry> tc;
    // MineGroup (sub_429970): mina/smelter +0x308/+0x30C, docas (px internos,
    // centro do tile) +0x310/+0x318, tipos +0x320/+0x324.
    int mine = 0, smelter = 0, mineDock[2] = {0, 0}, smelterDock[2] = {0, 0}, mineType = 0, smelterType = 0;
    int area[4] = {0, 0, 0, 0};                              // Mine +0x328 / Build +0x338 (px internos)
    // BuildGroup: listas e a máquina de reconstrução (+0x348..+0x358).
    std::vector<BuildRec> blds, mines;
    std::vector<TubeRec> tubes;
    std::vector<ReinfRec> reinf;
    int bState = 0, conVec = 0, factory = 0, bIndex = 0, bay = -1;

    bool hasClass(int c) const {
        for (const Node& n : nodes) if (n.cls == c) return true;
        return false;
    }
    int saveKind() const override { return 3; }
    void saveIo(SaveStream& s) override {
        s.io(kind); s.io(player); s.io(deleteAt); s.io(lights);
        s.ioVec(nodes);
        s.io(combine); s.io(combineTarget); s.io(combineTick);
        s.io(patrolN); s.io(patrolStart); s.io(patrolPts); s.io(patrolOn); s.io(follow);
        s.io(rect); s.io(targetGroup); s.io(attackType); s.io(targetUnit);
        s.ioVec(guarded);
        s.io(mode);
        s.io(hasTC); s.io(tcDirty); s.ioVec(tc);
        s.io(mine); s.io(smelter); s.io(mineDock); s.io(smelterDock); s.io(mineType); s.io(smelterType);
        s.io(area);
        s.ioVec(blds); s.ioVec(mines); s.ioVec(tubes); s.ioVec(reinf);
        s.io(bState); s.io(conVec); s.io(factory); s.io(bIndex); s.io(bay);
    }
    int firstOf(int c) const {
        for (size_t i = 0; i < nodes.size(); ++i) if (nodes[i].cls == c) return static_cast<int>(i);
        return -1;
    }
};

std::vector<int> s_groups;          // dword_4E0B64 (cabeça = mais novo)
// dword_4EFCD8 +0x1C/+0x20: por jogador, o grupo padrão e se ele veio da missão
// (SetDefaultGroup: 1 = não é apagado pelo jogo).
int s_defaultGroup[kMaxPlayers] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int s_defaultExternal[kMaxPlayers] = {};

Group* grp(int stub) { return dynamic_cast<Group*>(stubObject(stub)); }

Node* nodeOf(Group& g, int unit) {
    for (Node& n : g.nodes) if (n.unit == unit) return &n;
    return nullptr;
}

// ---- Ordens dos membros (pacotes do sub_40E210) --------------------------------
void orderStop(Node& n) {                                   // sub_42A190 (pacote 5)
    UnitRecord& r = g_units[n.unit];
    if (!r.action) return;
    r.action = 5; r.field20 = 1; r.field3C = 0;
    if (r.flags & UNIT_ARMED) unitResetCombat(n.unit);
}
void orderMove(Node& n, int ix, int iy) {                   // sub_429FB0 (pacote 2, px internos)
    n.tick = tick();
    CommandPacket pkt;
    pkt.type = CMD_MOVE;
    pkt.unitIds.push_back(n.unit);
    pkt.waypoints.push_back(packLocation(portTileX(ix), iy >> 5));
    processCommandPacket(ownerOf(g_units[n.unit]), pkt);
}
void orderAttack(Node& n, int target, int type) {           // sub_42A070 (0x24) / sub_42ABF0 (0x25)
    n.tick = tick();
    processAttackPacket(ownerOf(g_units[n.unit]), type, {n.unit}, {},
                        static_cast<uint32_t>(target) | 0xFFFF0000u);
}
void orderPatrol(Node& n, const Group& g) {                 // sub_42A5D0 (pacote 0x28)
    n.tick = tick();
    CommandPacket pkt;
    pkt.type = 0x28;
    pkt.unitIds.push_back(n.unit);
    pkt.waypoints.assign(g.patrolPts, g.patrolPts + g.patrolN);
    pkt.patrolStart = g.patrolStart;
    processCommandPacket(ownerOf(g_units[n.unit]), pkt);
}

// sub_44D480: destino do movimento (px internos).
bool moveDest(const UnitRecord& r, int& x, int& y) {
    if (!r.hasOrder || r.order.waypoints.empty()) return false;
    const uint32_t wp = r.order.waypoints.back();
    x = (unpackLocX(wp) + g_gameMap.internalXOffset()) * 32 + 16;
    y = unpackLocY(wp) * 32 + 16;
    return true;
}

// sub_42AA10: vai/fica na área; 1 = ordem dada ou já indo, 0 = ocioso dentro.
int keepInRect(Group& g, Node& n, const int rc[4], int force) {
    (void)g;
    if (n.cls > 9) return 0;
    UnitRecord& r = g_units[n.unit];
    // Andando há pouco (≤ 800 ticks) rumo a um ponto da área: deixa.
    if (r.action == CMD_MOVE && tick() - n.tick <= 0x320 && !force) {
        int dx, dy;
        if (moveDest(r, dx, dy) && inRect(rc, dx, dy)) return 1;
    }
    // Já dentro: fica (0) — a não ser que a última ordem tenha 8000+ ticks e
    // saia o 1/32 que manda vagar.
    int ux, uy;
    origPos(r, ux, uy);
    if (inRect(rc, ux, uy) && !force) {
        if (tick() - n.tick < 0x1F40) return 0;
        if (TethysGame::getRand(32) < 0x1F) return 0;
    }
    int w = wrapDX(rc[2] - rc[0]);
    int x = rc[0] + TethysGame::getRand(w);
    x &= pxMask();
    const int y = rc[1] + TethysGame::getRand(rc[3] - rc[1]);
    int tx = portTileX(x), ty = y >> 5;
    findVehicleSpot(r.type, tx, ty);                         // sub_4366D0
    n.tick = tick();
    CommandPacket pkt;
    pkt.type = CMD_MOVE;
    pkt.unitIds.push_back(n.unit);
    pkt.waypoints.push_back(packLocation(tx, ty));
    processCommandPacket(ownerOf(r), pkt);
    return 1;
}

// sub_42ABF0: escolta o membro mais próximo da classe `cat` (0x11 = qualquer).
void escort(Group& g, Node& n, int cat) {
    UnitRecord& r = g_units[n.unit];
    if (r.action == 0x25 && r.guardHi != 0x7FFF && valid(r.guardLo)) {
        const UnitRecord& gd = g_units[r.guardLo];
        if (gd.groupStub == g.id) {
            if (cat == 0x11) return;
            const Node* gn = nodeOf(g, r.guardLo);
            if (gn && gn->cls == cat) return;
        }
    }
    int best = -1;
    long long bestD = 0x7FFFFFFF;
    const int start = cat == 0x11 ? 0 : g.firstOf(cat);
    if (start >= 0)
        for (size_t i = static_cast<size_t>(start); i < g.nodes.size(); ++i) {
            if (g.nodes[i].cls > cat) break;
            const UnitRecord& o = g_units[g.nodes[i].unit];
            const long long dx = o.pixelX - r.pixelX, dy = o.pixelY - r.pixelY;
            const long long d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = g.nodes[i].unit; }
        }
    if (best < 0) { orderStop(n); return; }
    if (best == n.unit) return;                              // (escoltar a si mesmo: nada muda)
    orderAttack(n, best, 0x25);
}

// sub_42B4C0: atacantes recentes (≤ 30 ticks) dos membros, opcionalmente só
// dos membros dentro do retângulo.
void addAttackers(Group& g, TargetList& L, const int* rc);
void listAdd(TargetList& L, int id) {                       // sub_427F80
    if (L.count >= 16) return;
    const UnitRecord& t = g_units[id];
    L.u[L.count++] = id;
    if (t.flags & 0x80000u) {
        if (t.flags & UNIT_BUILDING) ++L.unpoweredOrEmpBld; else ++L.empVehicles;
    }
    if (t.flags & UNIT_BUILDING) {
        const StructSheetDesc* d = structSheet(t.type);
        const int power = d ? d->player[(t.owner >> 4) & 0x0F].power : 0;
        if (!power) ++L.unpoweredOrEmpBld; else ++L.foamedOrPowered;
    } else if (t.flags & 0x40000u) {
        ++L.foamedOrPowered;
    } else {
        ++L.vehicles;
    }
}
void addAttackers(Group& g, TargetList& L, const int* rc) {
    for (const Node& n : g.nodes) {
        const UnitRecord& m = g_units[n.unit];
        if (!m.lastAttacker || tick() - m.lastAttackTick > 0x1E) continue;
        if (rc) {
            int x, y;
            origPos(m, x, y);
            if (!inRect(rc, x, y)) continue;
        }
        const int a = m.lastAttacker;
        if (!valid(a) || (g_units[a].flags & UNIT_DEAD)) continue;
        if (allyOf(ownerOf(g_units[a]), g.player)) continue;
        listAdd(L, a);
    }
}

int distMaxMin(int x, int y, const UnitRecord& t) {
    int tx, ty;
    origPos(t, tx, ty);
    const int ady = std::abs(ty - y);
    const int adx = std::abs(wrapDX(x - tx));
    return adx < ady ? ady + adx / 2 : adx + ady / 2;
}

// sub_427850: alvo de combate.
int bestCombat(const TargetList& L, int x, int y, bool skipEmp) {
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < L.count; ++i) {
        const UnitRecord& t = g_units[L.u[i]];
        if (!(t.flags & UNIT_DAMAGEABLE)) continue;
        if (skipEmp && (t.flags & 0x80000u) && !(t.flags & UNIT_BUILDING)) continue;
        int d = distMaxMin(x, y, t);
        switch (t.type) {
            case 1: d += 0x60; break;
            case 2: case 4: case 0x17: break;
            case 3: d -= 0x60; break;
            case 5: case 6: case 7:
                if (t.cargo == 0x48 || t.cargo == 0x3B) d -= 0x60;
                break;
            case 0x0D: continue;
            default:
                if ((t.flags & UNIT_BUILDING) && !(t.flags & UNIT_AUTO_TARGET) && t.type != mapCommandCenter) continue;
                d = std::min(d * 2, d + 0x140);
                break;
        }
        if (d < bestD) { bestD = d; best = L.u[i]; }
    }
    return best;
}
// sub_427DC0: alvo do ESG (só veículos).
int bestEsg(const TargetList& L, int x, int y, bool skipEmp) {
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < L.count; ++i) {
        const UnitRecord& t = g_units[L.u[i]];
        if (!(t.flags & UNIT_DAMAGEABLE) || (t.flags & UNIT_BUILDING)) continue;
        if (skipEmp && (t.flags & 0x80000u)) continue;
        int d = distMaxMin(x, y, t);
        switch (t.type) {
            case 1: d += 0x60; break;
            case 3: d -= 0x60; break;
            case 4: case 0x17: break;
            case 5: case 6: case 7:
                if (t.cargo == 0x48 || t.cargo == 0x3B) d -= 0x60;
                break;
            case 0x0D: continue;
            default: d = std::min(d * 2, d + 0x140); break;
        }
        if (t.flags & 0x100000u) d += 0xA0;
        if (d < bestD) { bestD = d; best = L.u[i]; }
    }
    return best;
}
// sub_427A00: alvo do EMP.
int bestEmp(const TargetList& L, int x, int y) {
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < L.count; ++i) {
        const UnitRecord& t = g_units[L.u[i]];
        if (!(t.flags & UNIT_DAMAGEABLE) || t.type == 3 || t.type == 4) continue;
        if (t.flags & UNIT_BUILDING) {
            const StructSheetDesc* d = structSheet(t.type);
            if (!d || !d->player[(t.owner >> 4) & 0x0F].power) continue;
        }
        if (t.flags & 0x80000u) continue;
        int d = distMaxMin(x, y, t);
        if (t.type == 0x0D) continue;
        if (t.type == 0x17) d += 0x60;
        else if (!(t.type >= 4 && t.type <= 7) && (t.flags & UNIT_BUILDING)) {
            if (!(t.flags & UNIT_AUTO_TARGET) && t.type != mapCommandCenter) continue;
            d = std::min(d * 4, d + 0x280);
        }
        if (d < bestD) { bestD = d; best = L.u[i]; }
    }
    return best;
}
// sub_427BB0: alvo do Foam.
int bestFoam(const TargetList& L, int x, int y) {
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < L.count; ++i) {
        const UnitRecord& t = g_units[L.u[i]];
        if (!(t.flags & UNIT_DAMAGEABLE) || (t.flags & 0x40004u)) continue;
        int d = distMaxMin(x, y, t);
        if (t.type == 3) d -= 0x60;
        if (d < bestD) { bestD = d; best = L.u[i]; }
    }
    return best;
}
// sub_427CC0: alvo do Spider (veículo paralisado a até 256 px).
int bestSpider(const TargetList& L, int x, int y) {
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < L.count; ++i) {
        const UnitRecord& t = g_units[L.u[i]];
        if (!(t.flags & UNIT_DAMAGEABLE) || !(t.flags & UNIT_VEHICLE) || !(t.flags & 0x80000u)) continue;
        const int d = distMaxMin(x, y, t);
        if (d > 0x100) continue;
        const int s = d - t.empTicks;
        if (s < bestD) { bestD = s; best = L.u[i]; }
    }
    return best;
}

// Escolta padrão quando não há alvo (sub_428900 / sub_4289F0).
int fallbackCat(const Group& g, const TargetList& L) {
    if (g.hasClass(4) && L.empVehicles) return 4;
    if (g.hasClass(2) && (L.vehicles || L.foamedOrPowered)) return 2;
    if (g.hasClass(0)) return 0;
    if (g.hasClass(1)) return 1;
    if (g.hasClass(3) && L.foamedOrPowered != L.count) return 3;
    return 0x10;
}

// ---- Tratadores por modo (off_4E0A78 + modo*20) ---------------------------------
// A: o membro ocioso dentro da área.
void modeIdle(Group& g, Node& n) {
    switch (g.mode) {
        case 0: case 2: case 3: case 4: case 5: orderStop(n); break;   // sub_429130 / ...
        case 6:                                                      // sub_42A270 (0x2A)
            n.tick = tick();
            unitExitMap(n.unit);
            break;
        default: break;                                              // 1: nada
    }
}
// B: relevância do alvo (0 ignora, 1 sempre, 2 até 120 ticks, 3 até 60).
int modeTargetRank(const Group& g, const UnitRecord& t) {
    switch (g.mode) {
        case 0: {                                                    // sub_429140
            int x, y;
            origPos(t, x, y);
            for (const auto& r : g.guarded) if (inRect(r.data(), x, y)) return 1;
            return 2;
        }
        case 1: return 0;                                            // sub_429F40
        case 4: return g.targetUnit == t.id ? 1 : 2;                 // sub_424670
        case 5: return !g.attackType || t.type == g.attackType ? 1 : 2;   // sub_424460
        case 6: return 2;                                            // sub_428E10
        default: return 1;
    }
}
// C: coleta de alvos no nível 1..3.
void modeCollect(Group& g, int level, TargetList& L) {
    switch (g.mode) {
        case 0:                                                      // sub_4291C0
            if (level == 1) {
                for (const auto& r : g.guarded) {
                    MapRectIter it;
                    mapRectIterInit(it, r[0], r[1], wrapDX(r[2] - r[0]) + 1, r[3] - r[1] + 1,
                                    enemyMask(g.player), UNIT_BUILDING | UNIT_VEHICLE);
                    for (int id = mapRectIterNext(it); id; id = mapRectIterNext(it)) listAdd(L, id);
                }
            } else if (level == 2) {
                addAttackers(g, L, g.rect);
            }
            break;
        case 5:                                                      // sub_424490
            if (level == 1) {
                if (g.attackType) {
                    for (int id = unitListFirst(); id; id = unitListNext(id)) {
                        const UnitRecord& t = g_units[id];
                        if (t.id == -1 || (t.flags & UNIT_DEAD) || t.type != g.attackType) continue;
                        if (allyOf(ownerOf(t), g.player)) continue;
                        listAdd(L, id);
                        if (L.count == 16) break;
                    }
                    if (L.count) break;
                }
                // Os inimigos mais próximos do centro do grupo (sub_437BD0/C10).
                int cx = 0, cy = 0;
                if (!g.nodes.empty()) {
                    int sx = 0, sy = 0, x0, y0;
                    origPos(g_units[g.nodes[0].unit], x0, y0);
                    for (size_t i = 1; i < g.nodes.size(); ++i) {
                        int x, y;
                        origPos(g_units[g.nodes[i].unit], x, y);
                        sx += wrapDX(x - x0); sy += y;
                    }
                    sy += y0;
                    const int n = static_cast<int>(g.nodes.size());
                    cx = (x0 + sx / n) & pxMask(); cy = sy / n;
                }
                const uint32_t mask = enemyMask(g.player);
                std::vector<std::pair<long long, int>> c;
                for (int id = unitListFirst(); id; id = unitListNext(id)) {
                    const UnitRecord& t = g_units[id];
                    if (!(t.flags & 0x46u) || !(mask & (1u << ownerOf(t))) || !(t.flags & (UNIT_BUILDING | UNIT_VEHICLE)))
                        continue;
                    if (t.field22 == 0x0D || t.id == -1 || (t.flags & UNIT_DEAD)) continue;
                    int x, y;
                    origPos(t, x, y);
                    const long long dx = wrapDX(cx - x), dy = y - cy;
                    c.push_back({dx * dx + dy * dy, id});
                }
                std::stable_sort(c.begin(), c.end(),
                                 [](const auto& a, const auto& b) { return a.first < b.first; });
                for (const auto& e : c) { if (L.count >= 16) break; listAdd(L, e.second); }
            } else if (level == 2) {
                addAttackers(g, L, nullptr);
            }
            break;
        case 6:                                                      // sub_428E20
            if (level == 2) addAttackers(g, L, g.rect);
            break;
        default: break;
    }
}

// sub_4282C0: o alvo ainda vale p/ o membro da classe `cls` com a ordem de `age` ticks?
bool targetStillValid(const Group& g, int target, int cls, int age) {
    if (!valid(target)) return false;
    const UnitRecord& t = g_units[target];
    if (t.flags & UNIT_DEAD) return false;
    if (!(t.flags & UNIT_DAMAGEABLE)) return false;
    if (allyOf(ownerOf(t), g.player)) return false;
    const int r = modeTargetRank(g, t);
    if (!r) return false;
    if (r == 2 && age > 0x78) return false;
    if (r == 3 && age > 0x3C) return false;
    if (cls == 2) return !(t.flags & 0x80000u) || g.hasClass(4);
    if (cls == 1) return !(t.flags & 0x100000u);
    if (cls == 3) return !(t.flags & 0x40004u);
    if ((t.flags & 0x80000u) && g.hasClass(4)) return cls == 4;
    return true;
}

// Classes 5..9 e os sem alvo: patrulha ou fica na área (slots D/A).
void holdOrPatrol(Group& g, Node& n) {
    if (g.patrolOn) {
        if (g_units[n.unit].action != 0x28) orderPatrol(n, g);
        return;
    }
    if (!keepInRect(g, n, g.rect, 0)) modeIdle(g, n);
}

// funcs_428806[classe]
void classHandler(Group& g, Node& n, const TargetList& L) {
    UnitRecord& r = g_units[n.unit];
    int ux, uy;
    origPos(r, ux, uy);
    switch (n.cls) {
        case 0: {                                                    // sub_428900
            const int t = g.combine ? g.combineTarget : bestCombat(L, ux, uy, g.hasClass(4));
            if (t) { orderAttack(n, t, 0x24); return; }
            const int c = fallbackCat(g, L);
            if (c != 0x10) escort(g, n, c);
            return;
        }
        case 1: {                                                    // sub_4289F0
            const int t = bestEsg(L, ux, uy, g.hasClass(4));
            if (t) { orderAttack(n, t, 0x24); return; }
            const int c = fallbackCat(g, L);
            if (c != 0x10) escort(g, n, c);
            return;
        }
        case 2: {                                                    // sub_428AC0
            const int t = bestEmp(L, ux, uy);
            if (t) { orderAttack(n, t, 0x24); return; }
            escort(g, n, g.hasClass(4) ? 4 : 0);
            return;
        }
        case 3: {                                                    // sub_428B20
            const int t = bestFoam(L, ux, uy);
            if (t) { orderAttack(n, t, 0x24); return; }
            if (L.vehicles && g.hasClass(0)) escort(g, n, 0);
            else if (L.foamedOrPowered && g.hasClass(0)) escort(g, n, 0);
            else if (L.empVehicles && g.hasClass(4)) escort(g, n, 4);
            else orderStop(n);
            return;
        }
        case 4: {                                                    // sub_428BB0
            const int t = bestSpider(L, ux, uy);
            if (t) {                                                 // sub_42A100 (0x1D: tomar)
                n.tick = tick();
                // (tomar posse — pacote 0x1D ainda não portado)
                return;
            }
            if (!keepInRect(g, n, g.rect, 0)) modeIdle(g, n);
            return;
        }
        default: return;
    }
}

// sub_42B160: tira membros mortos ou que mudaram de dono.
void dropInvalid(Group& g) {                                // sub_42B160
    for (size_t i = 0; i < g.nodes.size();) {
        const UnitRecord& r = g_units[g.nodes[i].unit];
        if (r.id != -1 && !(r.flags & UNIT_DEAD) && ownerOf(r) == g.player) { ++i; continue; }
        UnitRecord& w = g_units[g.nodes[i].unit];
        w.groupStub = 0xFF;
        g.nodes.erase(g.nodes.begin() + static_cast<long>(i));
        g.tcDirty = true;
    }
}

// vt[13] (sub_428560).
void fightThink(Group& g) {
    if (g.nodes.empty()) return;
    // Classes 5..9: ficam na área / patrulham.
    for (Node& n : g.nodes) {
        if (n.cls < 5) continue;
        if (n.cls > 9) break;
        holdOrPatrol(g, n);
    }
    TargetList L;
    bool built = false;
    for (Node& n : g.nodes) {
        if (n.cls > 4) break;
        UnitRecord& r = g_units[n.unit];
        bool need = true;
        if (r.flags & 0xC0000u) need = false;
        else if ((r.action == 0x24 || r.action == 0x1D) && r.field4A != 0x7FFF && (r.orderTarget() & 0x80000000u)) {
            const int age = tick() - n.tick;
            if (targetStillValid(g, r.orderLo, n.cls, age) && age <= 0x190) need = false;
        }
        if (!need) continue;
        if (!built) {
            built = true;
            for (int lv = 1; lv <= 3; ++lv) {
                modeCollect(g, lv, L);
                if (L.count) break;
            }
            if (g.combine) {
                const int age = tick() - g.combineTick;
                if (!(g.combineTarget && targetStillValid(g, g.combineTarget, 0, age) && age <= 0x190)) {
                    int cx = 0, cy = 0;
                    {
                        int sx = 0, sy = 0, x0, y0;
                        origPos(g_units[g.nodes[0].unit], x0, y0);
                        for (size_t i = 1; i < g.nodes.size(); ++i) {
                            int x, y;
                            origPos(g_units[g.nodes[i].unit], x, y);
                            sx += wrapDX(x - x0); sy += y;
                        }
                        sy += y0;
                        const int k = static_cast<int>(g.nodes.size());
                        cx = (x0 + sx / k) & pxMask(); cy = sy / k;
                    }
                    g.combineTick = tick();
                    g.combineTarget = bestCombat(L, cx, cy, g.hasClass(4));
                }
            }
        }
        if (!L.count) holdOrPatrol(g, n);
        else classHandler(g, n, L);
    }
}

// sub_42B360: DeleteWhenEmpty — true = apagar o grupo.

// ================================================================================
// TargCount (+0x2E4, sub_42C1F0): tabela ordenada {tipo, arma, alvo, atual};
// arma 0x73 = qualquer (casa só pelo lado da entrada, sub_42CC40).
int tcCmp(const TCEntry& e, int type, int weapon) {
    const int d = e.type - type;
    if (d) return d;
    return e.weapon == 0x73 ? 0 : e.weapon - weapon;
}
// Busca binária de sub_42C3F0/sub_42C510/sub_42C8C0: índice ou −1.
int tcFind(const std::vector<TCEntry>& v, int type, int weapon) {
    if (v.empty()) return -1;
    int lo = 0, hi = static_cast<int>(v.size()) - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        const int c = tcCmp(v[mid], type, weapon);
        if (c > 0) hi = mid;
        else if (c < 0) lo = mid;
        else return mid;
    }
    if (tcCmp(v[lo], type, weapon) == 0) return lo;
    if (tcCmp(v[lo], type, weapon) > 0) return -1;
    if (tcCmp(v[hi], type, weapon) == 0) return hi;
    return -1;
}
// sub_42CA00: índice da entrada (insere ordenado se não houver; marca a tabela).
int tcEnsure(Group& g, int type, int weapon) {
    std::vector<TCEntry>& v = g.tc;
    int pos = 0;
    if (!v.empty()) {
        int lo = 0, hi = static_cast<int>(v.size()) - 1;
        bool found = false;
        while (hi - lo > 1) {
            const int mid = (lo + hi) / 2;
            const TCEntry key{type, weapon, 0, 0};
            const int c = tcCmp(v[mid], key.type, key.weapon);   // sub_42CC40(mid, chave)
            if (c > 0) hi = mid;
            else if (c < 0) lo = mid;
            else { pos = mid; found = true; break; }
        }
        if (found) return pos;
        const int cl = tcCmp(v[lo], type, weapon);
        if (cl == 0) return lo;
        if (cl > 0) pos = 0;
        else {
            const int ch = tcCmp(v[hi], type, weapon);
            if (ch == 0) return hi;
            pos = ch < 0 ? static_cast<int>(v.size()) : hi;
        }
    }
    v.insert(v.begin() + pos, TCEntry{type, weapon, 0, 0});
    g.tcDirty = true;
    return pos;
}
// sub_42C510: recontagem dos membros.
void tcRecount(Group& g) {
    for (TCEntry& e : g.tc) e.current = 0;
    for (const Node& n : g.nodes) {
        const UnitRecord& r = g_units[n.unit];
        const int k = tcFind(g.tc, r.type, static_cast<int16_t>(r.cargo));
        if (k >= 0) ++g.tc[k].current;
    }
    g.tcDirty = false;
}

// ---- Mapa em tiles INTERNOS ------------------------------------------------------
int imask() { return g_gameMap.internalWidth() - 1; }                    // dword_54EBC4
int toPortX(int ix) { return ((ix & imask()) - g_gameMap.internalXOffset()); }
bool onPortMap(int ix, int y) {
    const int px = toPortX(ix);
    return px >= 0 && px < g_gameMap.width() && y >= 0 && y < g_gameMap.height();
}
int occupantAt(int ix, int y) { return onPortMap(ix, y) ? g_gameMap.cellOccupant(toPortX(ix), y) : 0; }
int cellTypeAt(int ix, int y) { return onPortMap(ix, y) ? static_cast<int>(g_gameMap.rawCell(toPortX(ix), y) & 0x1F) : 0; }
// Ocupante da célula = prédio do jogador do grupo → id; senão 0.
int playerBuildingAt(const Group& g, int ix, int y) {
    const int id = occupantAt(ix, y);
    if (!id || !valid(id)) return 0;
    const UnitRecord& r = g_units[id];
    if (ownerOf(r) != g.player || !(r.flags & UNIT_BUILDING)) return 0;
    return id;
}
bool liveOwned(const Group& g, int id) {
    return valid(id) && !(g_units[id].flags & UNIT_DEAD) && ownerOf(g_units[id]) == g.player;
}
void dropRefOf(int id) {
    if (valid(id) && g_units[id].refCount) --g_units[id].refCount;
}

// sub_437A90/sub_437AF0: alguma unidade (flags 0x46, dono em pm, flags & fm)
// a menos de r px do ponto (px internos; x com volta).
bool anyInCircle(int px, int py, int r, uint32_t pm, uint32_t fm) {
    const int y0 = py - r, y1 = std::min(py + r, g_gameMap.height() * 32);
    for (int id = unitListFirst(); id; id = unitListNext(id)) {
        const UnitRecord& t = g_units[id];
        int ux, uy;
        origPos(t, ux, uy);
        if (uy < y0) continue;
        if (uy >= y1) break;
        if (!(t.flags & 0x46u) || !(pm & (1u << ownerOf(t))) || !(t.flags & fm) || t.field22 == 0x0D ||
            t.id == -1 || (t.flags & UNIT_DEAD))
            continue;
        const int dx = wrapDX(px - ux), dy = uy - py;
        if (static_cast<unsigned>(dx * dx + dy * dy) < static_cast<unsigned>(r * r)) return true;
    }
    return false;
}
// sub_42C130: nenhum armado inimigo (0x10|0x20|0x80) a 0x140 px do tile.
bool siteSafe(const Group& g, int ix, int iy) {
    return !anyInCircle(ix << 5, iy << 5, 0x140, enemyMask(g.player), 0x10u | 0x20u | 0x80u);
}

// Membros da classe cls (0x11 = todos) — os nós estão ordenados por classe.
template <class F> void forClass(Group& g, int cls, F f) {
    for (Node& n : g.nodes) {
        if (cls != 0x11 && n.cls < cls) continue;
        if (cls != 0x11 && n.cls > cls) break;
        if (!f(n)) break;
    }
}
int dist2(const UnitRecord& r, int px, int py) {
    int ux, uy;
    origPos(r, ux, uy);
    return (ux - px) * (ux - px) + (uy - py) * (uy - py);
}
Node* nearestOperating(Group& g, int px, int py, int cls) {            // sub_42C010
    Node* best = nullptr;
    int bd = 0x7FFFFFFF;
    forClass(g, cls, [&](Node& n) {
        const UnitRecord& r = g_units[n.unit];
        if (structureOperational(r)) {
            const int d = dist2(r, px, py);
            if (d < bd) { bd = d; best = &n; }
        }
        return true;
    });
    return best;
}
Node* nearestFree(Group& g, int px, int py, int cls) {                 // sub_42BFA0
    Node* best = nullptr;
    int bd = 0x7FFFFFFF;
    forClass(g, cls, [&](Node& n) {
        const UnitRecord& r = g_units[n.unit];
        if (!(r.flags & 0xC0000u) && r.field22 != 0x0A) {
            const int d = dist2(r, px, py);
            if (d < bd) { bd = d; best = &n; }
        }
        return true;
    });
    return best;
}
template <class P> Node* nearestWith(Group& g, P pred, int px, int py, int cls) {   // sub_42C0A0
    Node* best = nullptr;
    int bd = 0x7FFFFFFF;
    forClass(g, cls, [&](Node& n) {
        const UnitRecord& r = g_units[n.unit];
        if (!(r.flags & 0xC0000u)) {
            const int d = dist2(r, px, py);
            if (d < bd && pred(n)) { bd = d; best = &n; }
        }
        return true;
    });
    return best;
}
int countTypeCargo(Group& g, int type, int cargo) {                    // sub_42BEA0
    int c = 0;
    forClass(g, aiUnitClass(type, cargo), [&](Node& n) {
        const UnitRecord& r = g_units[n.unit];
        if (r.type == type && static_cast<int16_t>(r.cargo) == cargo) ++c;
        return true;
    });
    return c;
}
// Custo do tipo no bloco do jogador: comum +0x0C, raro +0x10.
bool affordable(int player, int type) {
    const uint8_t* b = unitStatBlock(type, player);
    if (!b) return false;
    int common, rare;
    std::memcpy(&common, b + 0x0C, 4);
    std::memcpy(&rare, b + 0x10, 4);
    const PlayerData& d = g_playerData[player];
    return d.rareOre >= rare && d.ore >= common;
}

// ---- Ordens (todas gravam o tick no nó) ------------------------------------------
void orderMoveDock(Node& n, int px, int py) {                          // sub_42A440 (pacote 3)
    n.tick = tick();
    CommandPacket pkt;
    pkt.type = 3;
    pkt.unitIds.push_back(n.unit);
    pkt.waypoints.push_back(packLocation(portTileX(px), py >> 5));
    processCommandPacket(ownerOf(g_units[n.unit]), pkt);
}
// vt[0x10C] da fábrica (sub_4158E0): esvazia a baia; com GORF operando, recicla.
void emptyBay(int factory, int bay) {                                  // pacote 0x0D
    UnitRecord& f = g_units[factory];
    const int p = ownerOf(f);
    const int type = static_cast<int8_t>(f.bayType[bay]);
    if (g_colony[p].stat(ST_GORF_GRP + 4)) {
        for (int b : colonyStructures(p)) {
            if (g_units[b].type != 0x39 || !structureOperational(g_units[b])) continue;
            gorfRecycle(b, type);
            if (type == 5 || type == 6 || type == 7 || type == 4 || type == 0x17)
                gorfRecycle(b, static_cast<int8_t>(f.bayCargo[bay]));
            break;
        }
    }
    f.bayType[bay] = 0;
    f.bayCargo[bay] = 0;
}
void orderProduce(Node& n, int type, int cargo, int groupStub) {      // sub_42A330
    UnitRecord& f = g_units[n.unit];
    int i = 0;
    while (i < 6 && f.bayType[i]) ++i;
    if (i == 6) { n.tick = tick(); emptyBay(n.unit, 0); }
    n.tick = tick();
    processProducePacket(ownerOf(f), n.unit, type, cargo, groupStub == 0xFF ? -1 : groupStub);
}
void orderBayTransfer(Node& n, int bay) {                              // sub_42A560 (pacote 0x0A)
    n.tick = tick();
    processBayTransfer(ownerOf(g_units[n.unit]), n.unit, bay);
}
// sub_42A6A0 (pacote 6): obra com o anel do registro; o prédio vai p/ o grupo.
void orderBuild(Node& n, const BuildRec& rec, int group) {
    n.tick = tick();
    const int m = imask();
    const uint32_t loc = packLocation(toPortX((rec.rc[2] + 1) & m), rec.rc[3] + 1);
    processBuildPacket(ownerOf(g_units[n.unit]), n.unit, loc, toPortX((rec.rc[0] - 1) & m), rec.rc[1] - 1,
                       toPortX((rec.rc[2] + 1) & m), rec.rc[3] + 1, group == 0xFF ? -1 : group);
}
// sub_42A800: longe (> 0x6400 px²) → anda; senão pacote 7 (uma célula).
void orderWall(Node& n, int x, int y, int mapId) {
    UnitRecord& r = g_units[n.unit];
    int ux, uy;
    origPos(r, ux, uy);
    const int dy = std::abs(uy - (y << 5)), dx = std::abs(wrapDX((x << 5) - ux));
    if (dx * dx + dy * dy > 0x6400) { orderMove(n, x << 5, y << 5); return; }
    n.tick = tick();
    earthworkWallOrder(n.unit, toPortX(x), y, toPortX(x + 1), y + 1, mapId);
}
// sub_42A200: despejar (pacote 0x0E) se ainda não despeja.
void orderDump(Node& n) {
    n.tick = tick();
    UnitRecord& r = g_units[n.unit];
    if (r.action == 0x0E) return;
    dumpCargoOrder(n.unit);
}

// ---- Limpeza do lote (sub_4273C0) e muros em volta (sub_4276B0) ------------------
bool clearSite(Group& g, const int rc[4], int except) {
    // (diferença em TILES com a volta em pixels — como o original)
    int dx = rc[2] - rc[0];
    if (-(pxMask() / 2) >= dx) dx += pxW();
    else if (pxMask() / 2 < dx) dx = -((-dx) & pxMask());
    std::vector<int> found;
    mapUnitsInRect(rc[0] << 5, rc[1] << 5, (dx + 1) << 5, (rc[3] - rc[1] + 1) << 5,
                   g_playerData[g.player].allyMask, 2u, found);
    bool any = false, init = false;
    int sx = 0, sy = 0, dir = 1, run = 0, vlen = 0, hlen = 0;
    const MAP_RECT& clip = g_gameMap.clipRect();      // dword_54EBD4..E0 (internos)
    for (int id : found) {
        UnitRecord& u = g_units[id];
        if (id == except || u.field22 == 1 || u.action != 0) continue;
        any = true;
        if (!init) {
            init = true;
            int h = rc[3] - rc[1];
            vlen = h + 4;
            hlen = wrapDX(h) + 4;                            // (BUG do original: pela altura)
            dir = 1;
            sy = rc[1] - 3;
            sx = (rc[0] - 2) & imask();
            run = vlen + 1;
        }
        pfSetContext(id);
        for (int guard = 0; guard < 4096; ++guard) {
            if (run == 0) {
                dir = (dir + 1) & 3;
                if (dir & 1) { run = vlen; ++vlen; } else { run = hlen; ++hlen; }
            } else {
                --run;
            }
            bool test = false;
            switch (dir) {
                case 0: sx = (sx - 1) & imask(); test = clip.x1 <= sx; break;
                case 1: ++sy; test = clip.y2 >= sy; break;
                case 2: sx = (sx + 1) & imask(); test = clip.x2 >= sx; break;
                case 3: --sy; test = clip.y1 <= sy; break;
            }
            if (!test || !onPortMap(sx, sy)) continue;
            if (g_gameMap.rawCell(toPortX(sx), sy) & 0x87FF0000u) continue;
            if (!pfCanEnter(toPortX(sx), sy)) continue;
            if (u.groupStub == 0xFF) {
                CommandPacket pkt;                              // Unit::DoMove
                pkt.type = CMD_MOVE;
                pkt.unitIds.push_back(id);
                pkt.waypoints.push_back(packLocation(toPortX(sx), sy));
                processCommandPacket(ownerOf(u), pkt);
            } else if (Group* og = grp(u.groupStub)) {
                if (Node* on = nodeOf(*og, id)) orderMove(*on, sx << 5, sy << 5);
            }
            break;
        }
    }
    return any;
}
void clearWallsAround(const int rc[4]) {
    const int xe = (rc[2] + 1) & imask();
    const int y0 = std::max(rc[1] - 1, 0), y1 = std::min(rc[3] + 1, g_gameMap.height() - 1);
    for (int x = (rc[0] - 1) & imask();; x = (x + 1) & imask()) {
        for (int y = y0; y <= y1; ++y) {
            const int ct = cellTypeAt(x, y);
            if (ct >= 0x17 && ct <= 0x19) destroyWallOrTube(toPortX(x), y);
        }
        if (x == xe) break;
    }
}

// ---- MineGroup ---------------------------------------------------------------------
// sub_437EB0/sub_437EE0: prédio (flags 4, do jogador) cujo footprint cobre a célula,
// entre os ocupantes da célula e das vizinhas esquerda/direita/acima.
template <class P> int buildingCovering(int ix, int iy, P pred) {
    const int cells[4][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}};
    const int n = iy > 0 ? 4 : 3;
    for (int k = 0; k < n; ++k) {
        const int id = occupantAt((ix + cells[k][0]) & imask(), iy + cells[k][1]);
        if (!id || !valid(id)) continue;
        const UnitRecord& r = g_units[id];
        if (r.flags & UNIT_DEAD) continue;
        if (!(r.flags & 6u) || !(r.flags & UNIT_BUILDING)) continue;
        int w, h;
        buildingFootprint(r.type, w, h);
        const int bx = ((unitOX(r) >> 5) + g_gameMap.internalXOffset()) & imask(), by = unitOY(r) >> 5;
        const int x1 = (bx - (w >> 1)) & imask(), y1 = by - (h >> 1);
        if (iy < y1 || iy > y1 + h - 1) continue;
        if (((ix - x1) & imask()) >= w) continue;
        if (pred(id)) return id;
    }
    return 0;
}
// sub_429C20 / sub_429DD0: acha a mina / o smelter pela doca e testa.
bool mineReady(Group& g) {
    if (!g.mine) {
        g.mine = buildingCovering(g.mineDock[0] >> 5, g.mineDock[1] >> 5, [&](int id) {
            const UnitRecord& r = g_units[id];
            int x, y;
            return ownerOf(r) == g.player && r.type == g.mineType && dockCell(id, x, y) &&
                   ((x + g_gameMap.internalXOffset()) & imask()) == (g.mineDock[0] >> 5) && y == (g.mineDock[1] >> 5);
        });
    }
    if (!g.mine || !structureOperational(g_units[g.mine])) return false;
    // Nenhum inimigo de classe <= 4 num quadrado de 0x140 px centrado na doca.
    std::vector<int> found;
    mapUnitsInRect((g.mineDock[0] - 0xA0) & pxMask(), g.mineDock[1] - 0xA0, 0x140, 0x140,
                   enemyMask(g.player), 0xFFFFFFFFu, found);
    for (int id : found)
        if (aiUnitClass(g_units[id].type, static_cast<int16_t>(g_units[id].cargo)) <= 4) return false;
    return true;
}
bool smelterReady(Group& g) {
    if (!g.smelter) {
        g.smelter = buildingCovering(g.smelterDock[0] >> 5, g.smelterDock[1] >> 5, [&](int id) {
            const UnitRecord& r = g_units[id];
            int x, y;
            return ownerOf(r) == g.player && r.type == g.smelterType && dockCell(id, x, y) &&
                   ((x + g_gameMap.internalXOffset()) & imask()) == (g.smelterDock[0] >> 5) &&
                   y == (g.smelterDock[1] >> 5);
        });
    }
    if (!g.smelter || !structureOperational(g_units[g.smelter])) return false;
    const PlayerData& d = g_playerData[g.player];
    const ColonyState& c = g_colony[g.player];
    if (g.mineType == 0x15) return c.oreCapacity > d.ore;          // +0x18 > +0x20
    return d.rareOre < c.rareCapacity;                            // +0x24 < +0x1C
}
void mineThink(Group& g) {                                         // vt[13] sub_429B00
    const int kind = g.mineType == 0x15 ? 2 : 3;
    int mineOk = -1, smelterOk = -1;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        Node& n = g.nodes[i];
        if (n.cls < 7) continue;
        if (n.cls > 7) break;
        UnitRecord& r = g_units[n.unit];
        if (r.field22 == 0x0D) continue;
        const int cargo = r.cargo & 0xF;
        const int* dock = nullptr;
        if (cargo == 0) {
            if (mineOk < 0) mineOk = mineReady(g) ? 1 : 0;
            if (mineOk) dock = g.mineDock;
        } else if (cargo != kind) {
            orderDump(n);
            continue;
        } else {
            if (smelterOk < 0) smelterOk = smelterReady(g) ? 1 : 0;
            if (smelterOk) dock = g.smelterDock;
        }
        if (!dock) { keepInRect(g, n, g.area, 0); continue; }
        if (r.action == 3 && tick() - n.tick <= 0x320) continue;
        orderMoveDock(n, dock[0], dock[1]);
    }
}

// ---- BuildGroup --------------------------------------------------------------------
int buildMinerType(int type) { return (type == 0x15 || type == 0x16 || type == 0x2A) ? 9 : type == 0x2C ? 0x0A : 0; }

int buildState0(Group& g) {                                         // sub_425C50
    if (TethysGame::rng().next() > 0x0FFFFFFFu) return 0;
    for (size_t i = 0; i < g.blds.size(); ++i) {
        const BuildRec& b = g.blds[i];
        if (playerBuildingAt(g, b.rc[2], b.rc[1])) continue;
        if (!siteSafe(g, b.rc[2], b.rc[3])) continue;
        g.bIndex = static_cast<int>(i);
        int found = 0;
        forClass(g, 5, [&](Node& n) {
            const UnitRecord& c = g_units[n.unit];
            if (static_cast<int16_t>(c.cargo) == b.type &&
                (b.type != 0x17 || static_cast<int8_t>(c.kitWeapon) == b.a14)) {
                found = n.unit;
                return false;
            }
            return true;
        });
        if (found) { g.bState = 3; g.conVec = found; }
        else g.bState = 1;
        return 1;
    }
    return 0;
}
void dockPx(int building, int& px, int& py) {
    int x, y;
    dockCell(building, x, y);
    px = ((x + g_gameMap.internalXOffset()) & imask()) * 32 + 16;
    py = y * 32 + 16;
}
int buildState1(Group& g) {                                         // sub_425DB0
    const BuildRec& b = g.blds[g.bIndex];
    if (!g.factory) {
        g.conVec = 0;
        Node* f = nearestOperating(g, b.rc[2] << 5, b.rc[3] << 5, 0x0C);
        if (!f) return 0;
        g.bay = -1;
        g.factory = f->unit;
    }
    int dx, dy;
    dockPx(g.factory, dx, dy);
    int cv = g.conVec;
    if (!cv) {
        Node* c = nearestFree(g, dx, dy, 5);
        if (c) { g.conVec = c->unit; cv = c->unit; }
    }
    bool atDock = false;
    if (cv) {
        UnitRecord& c = g_units[cv];
        int cx, cy;
        origPos(c, cx, cy);
        atDock = cx == dx && cy == dy;
        if (!atDock) {
            int wx, wy;
            const bool going = c.action == 3 && moveDest(c, wx, wy) && (wx >> 5) == (dx >> 5) && (wy >> 5) == (dy >> 5);
            if (!going) if (Node* cn = nodeOf(g, cv)) orderMoveDock(*cn, dx, dy);
        }
    }
    UnitRecord& f = g_units[g.factory];
    if (g.bay == -1) {
        for (int i = 0; i < 6; ++i)
            if (static_cast<int8_t>(f.bayType[i]) == b.type && static_cast<int8_t>(f.bayCargo[i]) == b.a14) {
                g.bay = i;
                break;
            }
        if (g.bay == -1) {
            if (!(f.action == 9 && f.produceType == b.type && static_cast<int8_t>(f.kitWeapon) == b.a14))
                if (Node* fn = nodeOf(g, g.factory)) orderProduce(*fn, b.type, b.a14, 0xFF);
            return 0;
        }
    }
    if (atDock) { g.bState = 2; return 1; }
    return 0;
}
int buildState2(Group& g) {                                         // sub_425FF0
    if (!g.conVec || !g.factory) { g.bay = -1; g.bState = 1; return 1; }
    UnitRecord& c = g_units[g.conVec];
    const BuildRec& b = g.blds[g.bIndex];
    if (static_cast<int16_t>(c.cargo) == b.type && static_cast<int8_t>(c.kitWeapon) == b.a14) {
        g.bState = 3;
        return 1;
    }
    int dx, dy;
    dockPx(g.factory, dx, dy);
    int cx, cy;
    origPos(c, cx, cy);
    if (cx != dx && cy != dy) { g.conVec = 0; g.bState = 1; return 1; }   // (o original exige os dois diferentes)
    if (c.field22 != 0x0D)
        if (Node* fn = nodeOf(g, g.factory)) orderBayTransfer(*fn, g.bay);
    return 0;
}
int buildState3(Group& g) {                                         // sub_426120
    BuildRec& b = g.blds[g.bIndex];
    if (playerBuildingAt(g, b.rc[2], b.rc[1])) {
        if (g.conVec) if (Node* cn = nodeOf(g, g.conVec)) keepInRect(g, *cn, g.area, 1);
        g.factory = 0; g.conVec = 0; g.bState = 0;
        return 1;
    }
    if (!g.conVec) { g.bay = -1; g.bState = 1; return 1; }
    const int tx = ((b.rc[2] + 1) & imask()) * 32 + 16, ty = b.rc[3] * 32 + 0x30;
    UnitRecord& c = g_units[g.conVec];
    int cx, cy;
    origPos(c, cx, cy);
    if (cx == tx && cy == ty) {
        if (c.action != 6) if (Node* cn = nodeOf(g, g.conVec)) orderBuild(*cn, b, b.a18);
        clearSite(g, b.rc, g.conVec);
        clearWallsAround(b.rc);
        return 0;
    }
    int wx, wy;
    const bool going = c.action == CMD_MOVE && moveDest(c, wx, wy) && (wx >> 5) == (tx >> 5) && (wy >> 5) == (ty >> 5);
    if (!going) {
        if (Node* cn = nodeOf(g, g.conVec)) orderMove(*cn, tx, ty);
        const int ct = cellTypeAt(tx >> 5, ty >> 5);
        if (ct >= 0x17 && ct <= 0x19) destroyWallOrTube(toPortX(tx >> 5), ty >> 5);
    }
    return 0;
}

void buildMines(Group& g) {                                         // sub_4269E0
    if (TethysGame::rng().next() > 0x0FFFFFFFu) return;
    for (BuildRec& m : g.mines) {
        const int want = buildMinerType(m.type);
        if (m.a18) {
            const int w = m.a18;
            if (liveOwned(g, w)) {
                const UnitRecord& u = g_units[w];
                if ((u.flags & UNIT_VEHICLE) && u.action == 6) {
                    clearSite(g, m.rc, w);
                    clearWallsAround(m.rc);
                    continue;
                }
                if ((u.flags & UNIT_BUILDING) && u.action == 9 && u.produceType == want) continue;
            }
            m.a18 = 0;
            dropRefOf(w);
        }
        if (playerBuildingAt(g, m.rc[2], m.rc[1])) continue;   // (a18 já é 0 aqui)
        if (!siteSafe(g, m.rc[2], m.rc[3])) continue;
        Node* n = nearestWith(g, [&](Node& c) {                  // loc_426DE0
            const UnitRecord& r = g_units[c.unit];
            return r.type == want && r.action != 6;
        }, m.rc[2], m.rc[3], 9);
        if (n) {
            orderBuild(*n, m, m.a14);
            m.a18 = n->unit;
            ++g_units[n->unit].refCount;
            continue;
        }
        if (countTypeCargo(g, want, 0)) continue;
        if (!affordable(g.player, want)) continue;
        bool done = false;
        forClass(g, 0x0A, [&](Node& f) {
            const UnitRecord& r = g_units[f.unit];
            if (!structureOperational(r) || r.action == 9) return true;
            orderProduce(f, want, 0, g.id);
            m.a18 = f.unit;
            ++g_units[f.unit].refCount;
            done = true;
            return false;
        });
        (void)done;
    }
}

struct ReinfItem { int r, type, weapon, cum; };
// sub_427080: distribui os itens pelas fábricas livres da classe.
void assignFactories(Group& g, std::vector<ReinfItem>& items, int sum, int cls) {
    const int n = static_cast<int>(items.size());
    int k = 0;
    forClass(g, cls, [&](Node& f) {
        const UnitRecord& r = g_units[f.unit];
        if (!structureOperational(r) || r.action == 9) return true;
        ++k;
        int idx = 0;
        if (n > k) {
            do {
                const int x = TethysGame::getRand(sum);
                idx = 0;
                while (items[idx].cum <= x) ++idx;
            } while (g.reinf[items[idx].r].factory != 0);
        } else {
            while (g.reinf[items[idx].r].factory != 0) ++idx;
        }
        const ReinfItem& it = items[idx];
        orderProduce(f, it.type, it.weapon, g.reinf[it.r].stub);
        g.reinf[it.r].factory = f.unit;
        ++g_units[f.unit].refCount;
        return n > k;
    });
}
void buildReinforce(Group& g) {                                     // sub_426E10
    if (TethysGame::rng().next() > 0x0FFFFFFFu) return;
    std::vector<ReinfItem> a, b;
    int sumA = 0, sumB = 0;
    for (size_t ri = 0; ri < g.reinf.size(); ++ri) {
        ReinfRec& e = g.reinf[ri];
        if (e.factory) {
            if (liveOwned(g, e.factory) && g_units[e.factory].action == 9) continue;
            const int f = e.factory;
            e.factory = 0;
            dropRefOf(f);
        }
        Group* t = grp(e.stub);
        if (!t || t->nodes.size() == 32 || !t->hasTC || t->tc.empty()) continue;
        if (t->tcDirty) tcRecount(*t);
        const int cnt = static_cast<int>(t->tc.size());
        const int start = TethysGame::getRand(cnt);
        int k = start;
        do {
            const TCEntry& c = t->tc[k];
            if (c.target > c.current) {
                if (c.type >= 3 && c.type <= 4) {
                    sumB += e.prio;
                    b.push_back({static_cast<int>(ri), c.type, c.weapon, sumB});
                    break;
                }
                if (c.type >= 1 && c.type <= 0x0F && a.size() < 16) {
                    sumA += e.prio;
                    a.push_back({static_cast<int>(ri), c.type, c.weapon, sumA});
                    break;
                }
            }
            if (++k == cnt) k = 0;
        } while (k != start);
    }
    if (!a.empty()) assignFactories(g, a, sumA, 0x0A);
    if (!b.empty()) assignFactories(g, b, sumB, 0x0B);
}

// sub_426360: os reparadores livres (classes 4..6) vão ao prédio danificado
// mais próximo (dano > HP/16, fora de obra, lugar seguro) — pacote 0x1C.
void buildRepair(Group& g) {
    if (TethysGame::rng().next() > 0x0FFFFFFFu) return;
    std::vector<Node*> rep;
    for (Node& n : g.nodes) {
        if (n.cls < 4) continue;
        if (n.cls > 6 || rep.size() >= 32) break;
        const UnitRecord& r = g_units[n.unit];
        if (n.unit == g.conVec || (r.flags & 0xC0000u) || r.action == 0x1C) continue;
        rep.push_back(&n);
    }
    if (rep.empty()) return;
    std::vector<int> dmg;
    auto consider = [&](const BuildRec& b) {
        if (dmg.size() >= 32) return;
        const int id = playerBuildingAt(g, b.rc[2], b.rc[1]);
        if (!id) return;
        const UnitRecord& r = g_units[id];
        if (!r.damage) return;
        const uint8_t* blk = unitStatBlock(r.type, (r.owner >> 4) & 0x0F);
        int hp = 0;
        if (blk) std::memcpy(&hp, blk, 4);
        if (hp / 16 >= r.damage || r.action == 0x21) return;
        int ux, uy;
        origPos(r, ux, uy);
        if (!siteSafe(g, ux >> 5, uy >> 5)) return;
        dmg.push_back(id);
    };
    for (const BuildRec& b : g.blds) consider(b);
    for (const BuildRec& b : g.mines) consider(b);
    if (dmg.empty()) return;
    for (Node* n : rep) {
        int best = 0, bd = 0x7FFFFFFF;
        int vx, vy;
        origPos(g_units[n->unit], vx, vy);
        for (int id : dmg) {
            int bx, by;
            origPos(g_units[id], bx, by);
            const int dx = wrapDX(bx - vx), dy = by - vy;
            const int d = dx * dx + dy * dy;
            if (d < bd) { bd = d; best = id; }
        }
        n->tick = tick();                                    // sub_42A980
        processRepairPacket(ownerOf(g_units[n->unit]), 0x1C, {n->unit}, {}, static_cast<uint32_t>(best) | 0xFFFF0000u);
    }
}

void buildTubes(Group& g) {                                         // sub_426720
    if (TethysGame::rng().next() > 0x0FFFFFFFu) return;
    for (TubeRec& t : g.tubes) {
        const int ct = cellTypeAt(t.x, t.y);
        if (t.worker) {
            if (ct >= 0x17 && ct <= 0x1A && t.type != ct) destroyWallOrTube(toPortX(t.x), t.y);
            const int rc[4] = {t.x, t.y, t.x, t.y};
            clearSite(g, rc, t.worker);
            if (liveOwned(g, t.worker) && g_units[t.worker].action == 7) continue;
            const int w = t.worker;
            t.worker = 0;
            dropRefOf(w);
        }
        if (ct == t.type) {
            if (t.worker) {
                if (Node* n = nodeOf(g, t.worker)) orderStop(*n);
                const int w = t.worker;
                t.worker = 0;
                dropRefOf(w);
            }
            continue;
        }
        const int mapId = t.type == 0x17 ? 0x12 : t.type == 0x18 ? 0x14 : t.type == 0x19 ? 0x13 : t.type == 0x1A ? 0x11 : 0;
        if (!affordable(g.player, mapId)) continue;
        Node* n = nearestWith(g, [&](Node& c) {                  // loc_4269C0
            return c.tick != tick() && g_units[c.unit].action != 7;
        }, t.x, t.y, 8);
        if (!n) continue;
        orderWall(*n, t.x, t.y, mapId);
        t.worker = n->unit;
        ++g_units[n->unit].refCount;
    }
}

void buildThink(Group& g) {                                         // vt[13] sub_425B30
    if (g.nodes.empty()) return;
    if (g.conVec && !liveOwned(g, g.conVec)) g.conVec = 0;
    if (g.factory && !liveOwned(g, g.factory)) g.factory = 0;
    for (int guard = 0; guard < 16; ++guard) {
        int more = 0;
        switch (g.bState) {
            case 0: more = buildState0(g); break;
            case 1: more = g.blds.empty() ? 0 : buildState1(g); break;
            case 2: more = g.blds.empty() ? 0 : buildState2(g); break;
            case 3: more = g.blds.empty() ? 0 : buildState3(g); break;
        }
        if (!more) break;
    }
    buildMines(g);
    buildReinforce(g);
    buildRepair(g);
    buildTubes(g);
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        Node& n = g.nodes[i];
        if (n.cls > 9) break;
        if (g_units[n.unit].action == 0 && n.unit != g.conVec) keepInRect(g, n, g.area, 0);
    }
}

// sub_4362D0 (Heckbert, por colunas) com sub_430010 (dentro: não visitado e
// prédio do jogador ou tubo) e sub_4300C0 (visita; grava a célula se não há
// prédio por cima, até `budget`). Coordenadas internas.
void tubeFill(int player, int x0, int y0, std::vector<std::pair<int, int>>& out, int budget) {
    const int W = g_gameMap.internalWidth(), H = g_gameMap.height(), m = imask();
    std::vector<uint8_t> seen(static_cast<size_t>(W) * H, 0);
    auto inside = [&](int x, int y) {
        x &= m;
        if (y < 0 || y >= H || seen[static_cast<size_t>(y) * W + x]) return false;
        const int id = occupantAt(x, y);
        if (id && valid(id) && (g_units[id].flags & UNIT_BUILDING)) return ownerOf(g_units[id]) == player;
        return cellTypeAt(x, y) == 0x1A;
    };
    auto set = [&](int x, int y) {
        x &= m;
        seen[static_cast<size_t>(y) * W + x] = 1;
        if (budget <= 0) return;
        if (buildingCovering(x, y, [](int) { return true; })) return;
        out.emplace_back(x, y);
        --budget;
    };
    struct Seg { int x, yl, yr, dx; };
    std::vector<Seg> st;
    inside(x0, y0);
    st.push_back({x0, y0, y0, 1});
    st.push_back({x0 - 1, y0, y0, -1});
    const size_t kMax = 0x3FF;
    while (!st.empty()) {
        Seg s = st.back();
        st.pop_back();
        const int x = s.x & m;
        int yl = s.yl, l = 0, y;
        int nx = x + s.dx;
        bool scan = false;
        if (inside(x, yl)) {
            int top = yl;
            while (top > 0 && inside(x, top - 1)) { --top; set(x, top); }
            if (yl > top) {
                if (st.size() < kMax) st.push_back({x - s.dx, top, yl - 1, -s.dx});
                l = top;
                scan = true;
            }
        }
        for (;;) {
            if (!scan) {
                while (yl <= s.yr && !inside(x, yl)) ++yl;
                if (yl > s.yr) break;
                l = yl;
            }
            scan = false;
            y = yl;
            do { set(x, y); ++y; } while (y <= s.yr && inside(x, y));
            if (y > s.yr && y < H && inside(x, y)) {
                while (y < H && inside(x, y)) { set(x, y); ++y; }
                --y;
                if (st.size() < kMax) st.push_back({nx, l, y, s.dx});
                if (y > s.yr && st.size() < kMax) st.push_back({x - s.dx, s.yr + 1, y, -s.dx});
                break;
            }
            yl = y + 1;
            if (st.size() < kMax) st.push_back({nx, l, y - 1, s.dx});
            if (yl > s.yr) break;
        }
    }
}

bool deleteCheck(Group& g) {
    if (g.deleteAt == 0x7FFFFFFF) return false;
    if (g.nodes.empty()) return true;
    // (o original, vencido o prazo, ordena aos membros que se autodestruam (0x14)
    // ou saiam do mapa (0x2A) — não portado.)
    return false;
}

} // namespace

// ================================================================================
int aiUnitClass(int type, int weapon) {                      // sub_49D1B0
    if (type >= 5 && type <= 7) {
        if (weapon == 0x46) return 1;
        if (weapon == 0x47) return 3;
        if (weapon == 0x3C) return 2;
        return 0;
    }
    switch (type) {
        case 4: return 0;
        case 3: return 4;
        case 2: return 5;
        case 0x0E: return 6;
        case 1: return 7;
        case 0x0F: return 8;
        case 0x15: case 0x16: return 0x0D;
        case 0x30: return 0x0A;
        case 0x2D: return 0x0B;
        case 0x2F: return 0x0C;
        case 0x17: return 0x0E;
        default: return (type >= 1 && type <= 0x0F) ? 9 : 0x0F;
    }
}

Stub* aiGroupNewForLoad() { return new Group(); }
void aiGroupsSerializeLists(SaveStream& s) {
    s.ioVec(s_groups);
    s.io(s_defaultGroup);
    s.io(s_defaultExternal);
}

void aiGroupsReset() {
    s_groups.clear();
    for (int& d : s_defaultGroup) d = 0xFF;
    for (int& e : s_defaultExternal) e = 0;
}

int aiCreateGroup(int kind, int player) {
    Group* g = new Group();
    g->kind = kind;
    g->player = player;
    g->enabled = 2;                                          // sub_42AFB0: [+8] = 2
    const int id = stubAllocate(g);
    if (id != 0xFF) s_groups.insert(s_groups.begin(), id);   // sub_47B4F0 (cabeça)
    return id;
}

bool aiIsGroup(int stub) { return grp(stub) != nullptr; }

void aiTakeUnit(int stub, int unitId) {                      // sub_42B960
    Group* g = grp(stub);
    if (!g || !alive(unitId)) return;
    if (g->nodes.size() >= 32) { unitExitMap(unitId); return; }   // cheio: pacote 0x2A
    UnitRecord& r = g_units[unitId];
    if (r.groupStub != 0xFF && r.groupStub != stub) aiRemoveUnit(r.groupStub, unitId);
    if (r.groupStub == stub) return;
    Node n;
    n.unit = unitId;
    n.cls = aiUnitClass(r.type, static_cast<int16_t>(r.cargo));
    n.tick = static_cast<int>(0xFFF00000u);
    // Inserção: logo depois do 1º da mesma classe; senão antes da próxima classe; senão no fim.
    auto& v = g->nodes;
    int pos = -1;
    for (size_t i = 0; i < v.size(); ++i) if (v[i].cls == n.cls) { pos = static_cast<int>(i) + 1; break; }
    if (pos < 0) {
        pos = static_cast<int>(v.size());
        for (size_t i = 0; i < v.size(); ++i) if (v[i].cls > n.cls) { pos = static_cast<int>(i); break; }
    }
    v.insert(v.begin() + pos, n);
    r.groupStub = static_cast<uint8_t>(stub);
    r.flags = (r.flags & ~UNIT_LIGHTS) | (g->lights & 1);   // luzes do grupo
    if (r.action) {                                          // STOP (pacote 5)
        r.action = 5; r.field20 = 1; r.field3C = 0;
        if (r.flags & UNIT_ARMED) unitResetCombat(unitId);
    }
    if (g->hasTC && !g->tcDirty) {                           // sub_42C3F0
        const int k = tcFind(g->tc, r.type, static_cast<int16_t>(r.cargo));
        if (k >= 0) ++g->tc[k].current;
    }
}

void aiRemoveUnit(int stub, int unitId) {                    // sub_42BBF0
    Group* g = grp(stub);
    if (!g || !valid(unitId)) return;
    auto& v = g->nodes;
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].unit == unitId) { v.erase(v.begin() + static_cast<long>(i)); break; }
    g_units[unitId].groupStub = 0xFF;
    g->tcDirty = true;
}

int aiAddUnitsFromBlock(int stub, const MissionUnitRecord* recs, int n, int (*portX)(int)) {   // sub_42BD50
    Group* g = grp(stub);
    if (!g) return 0;
    int made = 0;
    for (int i = 0; i < n && g->nodes.size() < 32; ++i) {
        const MissionUnitRecord& rc = recs[i];
        const int id = createUnitFromRecord(rc, portX(rc.x), rc.y, g->player, 0);
        if (id) { aiTakeUnit(stub, id); ++made; }
    }
    return made;
}

void aiSetLights(int stub, int on) {
    Group* g = grp(stub);
    if (!g) return;
    g->lights = on;
    for (const Node& n : g->nodes) {
        UnitRecord& r = g_units[n.unit];
        r.flags = (r.flags & ~UNIT_LIGHTS) | (on & 1);
    }
}

void aiSetDeleteWhenEmpty(int stub, int on) {                // sub_42B330
    if (Group* g = grp(stub)) g->deleteAt = on ? tick() + 10000 : 0x7FFFFFFF;
}

int aiUnitCount(int stub, int cls) {                         // sub_42BE60
    const Group* g = grp(stub);
    if (!g) return 0;
    if (cls == 0x11) return static_cast<int>(g->nodes.size());
    int c = 0;
    for (const Node& n : g->nodes) if (n.cls == cls) ++c;
    return c;
}
int aiTotalUnitCount(int stub) { const Group* g = grp(stub); return g ? static_cast<int>(g->nodes.size()) : 0; }

// ScGroup::HasBeenAttacked (sub_42BDA0): algum membro foi atacado nos últimos
// 30 ticks por uma unidade viva, sem a flag 0x20000, que não é aliada do dono.
bool aiGroupHasBeenAttacked(int stub) {
    const Group* g = grp(stub);
    if (!g) return false;
    for (const Node& n : g->nodes) {
        const UnitRecord& m = g_units[n.unit];
        if (!m.lastAttacker || tick() - m.lastAttackTick > 0x1E) continue;
        const int a = m.lastAttacker;
        if (!valid(a) || (g_units[a].flags & UNIT_DEAD)) continue;   // +4==-1 / flag 0x20000
        if (allyOf(ownerOf(g_units[a]), g->player)) continue;
        return true;
    }
    return false;
}
int aiFirstOfType(int stub, int cls) {
    const Group* g = grp(stub);
    if (!g) return 0;
    for (const Node& n : g->nodes) if (n.cls == cls) return n.unit;
    return 0;
}

static void resetTicks(Group& g) {                           // vt[9] sub_428150
    for (Node& n : g.nodes) n.tick = static_cast<int>(0xFFF00000u);
    g.combineTick = static_cast<int>(0xFFF00000u);
}

void aiFightSetRect(int stub, int x1, int y1, int x2, int y2) {
    Group* g = grp(stub);
    if (!g) return;
    resetTicks(*g);
    g->rect[0] = x1 << 5; g->rect[1] = y1 << 5; g->rect[2] = x2 << 5; g->rect[3] = y2 << 5;
    if (g->mode == 0 && g->guarded.size() < 8)                // GuardRect: também é guardado
        g->guarded.push_back({g->rect[0], g->rect[1], g->rect[2], g->rect[3]});
}
void aiFightSetRectPx(int stub, const int rc[4], bool guard) {
    Group* g = grp(stub);
    if (!g) return;
    resetTicks(*g);                                          // vt[9]
    for (int k = 0; k < 4; ++k) g->rect[k] = rc[k];
    if (guard) g->guarded.push_back({rc[0], rc[1], rc[2], rc[3]});
}
void unitExitMap(int unitId) {
    if (!valid(unitId)) return;
    UnitRecord& r = g_units[unitId];
    if (r.id == -1 || (r.flags & UNIT_DEAD)) return;
    r.flags |= UNIT_DEAD;
    r.action = 0x2A;
    r.field22 = SUB_REMOVE;
}
void aiFightAddGuardedRect(int stub, int x1, int y1, int x2, int y2) {
    Group* g = grp(stub);
    if (g && g->guarded.size() < 8) g->guarded.push_back({x1 << 5, y1 << 5, x2 << 5, y2 << 5});
}
void aiFightClearGuardedRects(int stub) { if (Group* g = grp(stub)) g->guarded.clear(); }

void aiFightSetMode(int stub, int mode) {                    // sub_428470
    Group* g = grp(stub);
    if (!g) return;
    g->combine = 0; g->follow = 0; g->patrolOn = 0; g->attackType = 0;
    g->guarded.clear();
    g->targetGroup = -1; g->targetUnit = -1; g->rect[1] = -1;
    resetTicks(*g);
    g->mode = mode;
}
void aiFightSetAttackType(int stub, int type) { if (Group* g = grp(stub)) { resetTicks(*g); g->attackType = type; } }
void aiFightSetTargetUnit(int stub, int u) { if (Group* g = grp(stub)) { resetTicks(*g); g->targetUnit = u; } }
void aiFightSetTargetGroup(int stub, int t) { if (Group* g = grp(stub)) { resetTicks(*g); g->targetGroup = t; } }
void aiFightSetFollowMode(int stub, int m) { if (Group* g = grp(stub)) { resetTicks(*g); g->follow = m; } }
void aiFightSetCombineFire(int stub, bool on) {
    if (Group* g = grp(stub)) { resetTicks(*g); g->combine = on ? 1 : 0; g->combineTarget = 0; }
}
void aiFightSetPatrol(int stub, int n, int start, const int* xs, const int* ys) {   // sub_4284D0
    Group* g = grp(stub);
    if (!g) return;
    resetTicks(*g);
    g->patrolOn = 1;
    g->patrolStart = start;
    g->patrolN = std::min(n, 8);
    for (int i = 0; i < g->patrolN; ++i) g->patrolPts[i] = packLocation(xs[i], ys[i]) | 0x60000000u;
}
void aiFightClearPatrol(int stub) { if (Group* g = grp(stub)) { resetTicks(*g); g->patrolOn = 0; } }

// sub_4035A0: o grupo padrão criado pelo jogo é destruído (vt[0]) e o da missão
// passa a valer, marcado como externo.
void aiSetDefaultGroup(int player, int stub) {
    if (player < 0 || player >= kMaxPlayers) return;
    if (!s_defaultExternal[player] && grp(s_defaultGroup[player])) {
        aiGroupDestroyed(s_defaultGroup[player]);
        stubDestroy(s_defaultGroup[player]);
    }
    s_defaultGroup[player] = stub;
    s_defaultExternal[player] = 1;
}
int aiDefaultGroup(int player) { return player >= 0 && player < kMaxPlayers ? s_defaultGroup[player] : 0xFF; }

// sub_402E70: CombatBase (FightGroup) do jogador de computador em modo 5
// (AttackEnemy). A área é o clipRect do mapa (dword_54EBD4..E0) com x1/y1 >= 0,
// x2 <= dword_54EBC4 e y2 <= altura − 1, gravada como está em +0x344: em TILES
// num campo que o resto do grupo lê em pixels (assim no original).
void aiDefaultGroupCreate(int player) {
    if (player < 0 || player >= kMaxPlayers) return;
    const int stub = aiCreateGroup(GK_FIGHT, player);
    s_defaultGroup[player] = stub;
    s_defaultExternal[player] = 0;
    Group* g = grp(stub);
    if (!g) return;
    aiFightSetMode(stub, 5);                                 // sub_428470(5)
    resetTicks(*g);                                          // vt[9]
    g->attackType = 0;                                       // [+0x358]
    const MAP_RECT& c = g_gameMap.clipRect();
    int rc[4] = {c.x1 > 0 ? c.x1 : 0, c.y1 > 0 ? c.y1 : 0, c.x2, c.y2};
    if (rc[2] >= imask()) rc[2] = imask();
    if (rc[3] >= g_gameMap.height() - 1) rc[3] = g_gameMap.height() - 1;
    resetTicks(*g);                                          // vt[9]
    for (int k = 0; k < 4; ++k) g->rect[k] = rc[k];
}

// sub_402E10 (antes do InitProc): grupo padrão p/ cada um dos 6 jogadores que
// não é humano ([+0x28] == 0); os humanos ficam sem (−1).
void aiDefaultGroupsStart() {
    for (int p = 0; p < 6; ++p) {
        if (!g_playerData[p].isHuman) aiDefaultGroupCreate(p);
        else s_defaultGroup[p] = 0xFF;
    }
}

// sub_402F50 (GoHuman): apaga o grupo padrão criado pelo jogo; o da missão só é
// esquecido.
void aiDefaultGroupDelete(int player) {
    if (player < 0 || player >= kMaxPlayers || s_defaultGroup[player] == 0xFF) return;
    if (!s_defaultExternal[player] && grp(s_defaultGroup[player])) {
        aiGroupDestroyed(s_defaultGroup[player]);
        stubDestroy(s_defaultGroup[player]);
    }
    s_defaultGroup[player] = 0xFF;
    s_defaultExternal[player] = 0;
}

// sub_403080 (fim do vt[0x68] de veículos e prédios): a unidade sai do grupo em
// que estava (vt[11]) e, se o novo dono é computador, entra no grupo padrão dele
// (vt[10]).
void aiUnitOwnerChanged(int unitId, int player) {
    if (!valid(unitId)) return;
    const int cur = g_units[unitId].groupStub;
    if (cur != 0xFF) aiRemoveUnit(cur, unitId);
    if (player < 0 || player >= kMaxPlayers || g_playerData[player].isHuman) return;
    if (s_defaultGroup[player] != 0xFF) aiTakeUnit(s_defaultGroup[player], unitId);
}

void aiGroupsDump() {
    for (int id : s_groups) {
        const Group* g = grp(id);
        if (!g) continue;
        int moving = 0, attacking = 0, patrolling = 0;
        for (const Node& n : g->nodes) {
            const int a = g_units[n.unit].action;
            if (a == CMD_MOVE) ++moving; else if (a == 0x24) ++attacking; else if (a == 0x28) ++patrolling;
        }
        std::printf("[AI] grupo %d tipo %d jogador %d membros %d modo %d rect (%d,%d)-(%d,%d) guardados %d "
                    "patrulha %d | andando %d atacando %d patrulhando %d\n", id, g->kind, g->player,
                    static_cast<int>(g->nodes.size()), g->mode, g->rect[0] >> 5, g->rect[1] >> 5, g->rect[2] >> 5,
                    g->rect[3] >> 5, static_cast<int>(g->guarded.size()), g->patrolOn, moving, attacking, patrolling);
        if (g->kind == GK_MINE) {
            int loaded = 0, docked = 0;
            for (const Node& n : g->nodes) {
                if (g_units[n.unit].cargo & 0xF) ++loaded;
                if (g_units[n.unit].field22 == 0x0D) ++docked;
            }
            std::printf("[AI]   mina %d (tipo 0x%X) smelter %d | carregados %d na doca %d | minerio j%d %d/%d raro %d/%d\n",
                        g->mine, g->mineType, g->smelter, loaded, docked, g->player, g_playerData[g->player].ore,
                        g_colony[g->player].oreCapacity, g_playerData[g->player].rareOre, g_colony[g->player].rareCapacity);
        }
        if (g->kind == GK_BUILD) {
            int missing = 0, tubesMissing = 0;
            for (const BuildRec& b : g->blds) if (!playerBuildingAt(*g, b.rc[2], b.rc[1])) ++missing;
            for (const TubeRec& t : g->tubes) if (cellTypeAt(t.x, t.y) != t.type) ++tubesMissing;
            std::printf("[AI]   predios %d (faltam %d) minas %d tubos %d (faltam %d) reforcos %d | estado %d convec %d fabrica %d baia %d\n",
                        static_cast<int>(g->blds.size()), missing, static_cast<int>(g->mines.size()),
                        static_cast<int>(g->tubes.size()), tubesMissing, static_cast<int>(g->reinf.size()), g->bState,
                        g->conVec, g->factory, g->bay);
        }
    }
}

void aiGroupsTick() {
    // vt[12] (sub_4281B0) + DeleteWhenEmpty (sub_42B360).
    for (int id : std::vector<int>(s_groups)) {
        Group* g = grp(id);
        if (!g) continue;
        dropInvalid(*g);
        if (g->kind == GK_MINE) {                            // vt[12] sub_429840
            if (g->mine && !liveOwned(*g, g->mine)) g->mine = 0;
            if (g->smelter && !liveOwned(*g, g->smelter)) g->smelter = 0;
        }
        if (g->kind == GK_FIGHT && g->targetUnit > 0) {
            const int t = g->targetUnit;
            if (!valid(t) || (g_units[t].flags & UNIT_DEAD)) g->targetUnit = 0;
        }
        if (deleteCheck(*g)) {
            for (const Node& n : g->nodes) g_units[n.unit].groupStub = 0xFF;
            s_groups.erase(std::find(s_groups.begin(), s_groups.end(), id));
            // (o objeto continua na tabela de stubs até o reset; a DLL só guarda o índice)
            g->enabled = 0;
        }
    }
    // vt[13] dos habilitados.
    for (int id : std::vector<int>(s_groups)) {
        Group* g = grp(id);
        if (!g || !g->enabled) continue;
        if (g->kind == GK_FIGHT) fightThink(*g);
        else if (g->kind == GK_MINE) mineThink(*g);
        else if (g->kind == GK_BUILD) buildThink(*g);
    }
}

void aiSetTargCount(int stub, int type, int weapon, int count) {   // sub_42B050 + sub_42C8C0
    Group* g = grp(stub);
    if (!g) return;
    g->hasTC = true;
    if (type < 1 || type > 0x0F) return;
    const int k = tcEnsure(*g, type, weapon ? weapon : 0x73);
    g->tc[k].target = count;
}
void aiSetTargCountBlock(int stub, const MissionUnitRecord* recs, int n) {   // sub_42CBE0
    Group* g = grp(stub);
    if (!g) return;
    g->hasTC = true;
    for (int i = 0; i < n; ++i) {
        if (recs[i].unitClass > 9) continue;
        const int k = tcEnsure(*g, recs[i].type, recs[i].weapon ? recs[i].weapon : 0x73);
        ++g->tc[k].target;
    }
}
int aiGroupMemberAt(int stub, int idx) {
    const Group* g = grp(stub);
    if (!g || idx < 0 || idx >= static_cast<int>(g->nodes.size())) return 0;
    return g->nodes[idx].unit;
}
void aiGroupDestroyed(int stub) {
    Group* g = grp(stub);
    if (!g) return;
    for (const Node& n : g->nodes) if (valid(n.unit)) g_units[n.unit].groupStub = 0xFF;
    g->nodes.clear();
    auto it = std::find(s_groups.begin(), s_groups.end(), stub);
    if (it != s_groups.end()) s_groups.erase(it);
}

void aiMineSetup(int stub, int mineId, int smelterId, int x1, int y1, int x2, int y2) {   // sub_429970
    Group* g = grp(stub);
    if (!g || !valid(mineId) || !valid(smelterId)) return;
    g->mine = mineId; g->smelter = smelterId;
    g->area[0] = x1 << 5; g->area[1] = y1 << 5; g->area[2] = x2 << 5; g->area[3] = y2 << 5;
    g->mineType = g_units[mineId].type;
    g->smelterType = g_units[smelterId].type;
    dockPx(mineId, g->mineDock[0], g->mineDock[1]);
    dockPx(smelterId, g->smelterDock[0], g->smelterDock[1]);
}

void aiBuildSetRect(int stub, int x1, int y1, int x2, int y2) {     // sub_425AB0
    if (Group* g = grp(stub)) { g->area[0] = x1 << 5; g->area[1] = y1 << 5; g->area[2] = x2 << 5; g->area[3] = y2 << 5; }
}
// sub_408880: footprint (tiles internos) do tipo centrado em (x, y).
static void footprintAt(int type, int x, int y, int rc[4]) {
    int w, h;
    buildingFootprint(static_cast<map_id>(type), w, h);
    rc[0] = (x - w / 2) & imask();
    rc[1] = std::max(y - h / 2, 0);
    rc[2] = (w + rc[0] - 1) & imask();
    rc[3] = std::min(h + rc[1] - 1, g_gameMap.height() - 1);
}
void aiBuildRecordBuilding(int stub, int x, int y, int type, int cargo, int group) {   // sub_4252A0
    Group* g = grp(stub);
    if (!g) return;
    BuildRec r;
    footprintAt(type, x, y, r.rc);
    r.type = type;
    if (type == 0x15 || type == 0x16 || type == 0x2C || type == 0x2A) {
        r.a14 = group; r.a18 = 0;
        g->mines.push_back(r);
    } else {
        r.a14 = cargo; r.a18 = group;
        g->blds.push_back(r);
    }
}
void aiBuildRecordBlock(int stub, const MissionUnitRecord* recs, int n, int first, int group) {   // sub_4254F0
    for (int i = first; i < n; ++i) aiBuildRecordBuilding(stub, recs[i].x, recs[i].y, recs[i].type, recs[i].weapon, group);
}
void aiBuildRecordTube(int stub, int x, int y) {                    // sub_425090
    if (Group* g = grp(stub)) g->tubes.push_back(TubeRec{x, y, 0x1A, 0});
}
void aiBuildRecordTubesTouching(int stub, int x, int y) {           // sub_4257D0
    Group* g = grp(stub);
    if (!g) return;
    std::vector<std::pair<int, int>> cells;
    tubeFill(g->player, x, y, cells, 0x800);
    for (const auto& c : cells) g->tubes.push_back(TubeRec{c.first, c.second, 0x1A, 0});
}
void aiBuildRecordReinforce(int stub, int group, int prio) {        // sub_4258E0
    if (Group* g = grp(stub)) g->reinf.push_back(ReinfRec{group, prio, 0});
}
// sub_425170: muro/tubo na lista de +0x31C, com o tipo de célula do map_id
// (0x11 tubo → 0x1A, 0x12 → 0x17, 0x13 → 0x19, 0x14 → 0x18; outro → 0).
void aiBuildRecordWall(int stub, int x, int y, int type) {
    Group* g = grp(stub);
    if (!g) return;
    const int cell = type == 0x11 ? 0x1A : type == 0x12 ? 0x17 : type == 0x13 ? 0x19 : type == 0x14 ? 0x18 : 0;
    g->tubes.push_back(TubeRec{x, y, cell, 0});
}
// sub_4259A0: tira o 1º registro de reforço do grupo; a fábrica dele perde a
// referência ([+0x32]).
void aiBuildUnRecordReinforce(int stub, int group) {
    Group* g = grp(stub);
    if (!g) return;
    for (size_t i = 0; i < g->reinf.size(); ++i) {
        if (g->reinf[i].stub != group) continue;
        if (g->reinf[i].factory) dropRefOf(g->reinf[i].factory);
        g->reinf.erase(g->reinf.begin() + static_cast<long>(i));
        return;
    }
}
void aiMineSetupLocs(int stub, int mx, int my, int sx, int sy, int mineType, int smelterType,
                     int x1, int y1, int x2, int y2) {      // sub_429A50
    Group* g = grp(stub);
    if (!g) return;
    g->area[0] = x1 << 5; g->area[1] = y1 << 5; g->area[2] = x2 << 5; g->area[3] = y2 << 5;
    g->mine = 0; g->smelter = 0;
    g->mineDock[0] = (mx << 5) + 0x10; g->mineDock[1] = (my << 5) + 0x10;
    g->smelterDock[0] = (sx << 5) + 0x10; g->smelterDock[1] = (sy << 5) + 0x10;
    g->mineType = mineType; g->smelterType = smelterType;
}
int aiFirstOfTypeExact(int stub, int type, int cargo) {
    Group* g = grp(stub);
    if (!g) return 0;
    const int cls = aiUnitClass(type, cargo);
    int found = 0;
    forClass(*g, cls, [&](Node& n) {
        const UnitRecord& r = g_units[n.unit];
        if (r.type == type && (cargo == 0 || static_cast<int16_t>(r.cargo) == cargo)) { found = n.unit; return false; }
        return true;
    });
    return found;
}
void aiTakeAllUnits(int stub, int fromStub) {               // sub_42BD00
    Group* from = grp(fromStub);
    if (!from || !grp(stub)) return;
    while (!from->nodes.empty()) {
        const int u = from->nodes.front().unit;
        aiRemoveUnit(fromStub, u);                          // vt[11] do outro
        aiTakeUnit(stub, u);                                // vt[10] deste
    }
}
void aiClearTargCount(int stub) {                           // loc_42B020
    if (Group* g = grp(stub)) { g->hasTC = false; g->tcDirty = false; g->tc.clear(); }
}

} // namespace op2
