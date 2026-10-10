#include "ai_group.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <vector>

#include "building_sheet.h"
#include "command.h"
#include "commands.h"
#include "game_map.h"
#include "player.h"
#include "scenario.h"
#include "tethys_game.h"
#include "trigger.h"
#include "unit.h"
#include "weapon.h"

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

    bool hasClass(int c) const {
        for (const Node& n : nodes) if (n.cls == c) return true;
        return false;
    }
    int firstOf(int c) const {
        for (size_t i = 0; i < nodes.size(); ++i) if (nodes[i].cls == c) return static_cast<int>(i);
        return -1;
    }
};

std::vector<int> s_groups;          // dword_4E0B64 (cabeça = mais novo)
int s_defaultGroup[kMaxPlayers] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
        case 6: {                                                    // sub_42A270 (0x2A)
            // (sair do mapa: pacote 0x2A — não portado)
            break;
        }
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
void dropInvalid(Group& g) {
    for (size_t i = 0; i < g.nodes.size();) {
        const UnitRecord& r = g_units[g.nodes[i].unit];
        if (r.id != -1 && !(r.flags & UNIT_DEAD) && ownerOf(r) == g.player) { ++i; continue; }
        UnitRecord& w = g_units[g.nodes[i].unit];
        w.groupStub = 0xFF;
        g.nodes.erase(g.nodes.begin() + static_cast<long>(i));
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

void aiGroupsReset() {
    s_groups.clear();
    for (int& d : s_defaultGroup) d = 0xFF;
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
    if (g->nodes.size() >= 32) return;                       // (cheio: pacote 0x2A — não portado)
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
}

void aiRemoveUnit(int stub, int unitId) {                    // sub_42BBF0
    Group* g = grp(stub);
    if (!g || !valid(unitId)) return;
    auto& v = g->nodes;
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].unit == unitId) { v.erase(v.begin() + static_cast<long>(i)); break; }
    g_units[unitId].groupStub = 0xFF;
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

void aiSetDefaultGroup(int player, int stub) { if (player >= 0 && player < kMaxPlayers) s_defaultGroup[player] = stub; }
int aiDefaultGroup(int player) { return player >= 0 && player < kMaxPlayers ? s_defaultGroup[player] : 0xFF; }

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
    }
}

void aiGroupsTick() {
    // vt[12] (sub_4281B0) + DeleteWhenEmpty (sub_42B360).
    for (int id : std::vector<int>(s_groups)) {
        Group* g = grp(id);
        if (!g) continue;
        dropInvalid(*g);
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
    }
}

} // namespace op2
