#include "movement.h"

#include <algorithm>
#include <cstdlib>

#include "unit.h"
#include "game_map.h"
#include "pathfinder.h"
#include "command.h"
#include "terrain_cost.h"
#include "building_sheet.h"
#include "structure.h"

namespace op2 {

// Aplica 1 tick do passo em curso — sub_44C0B0 loc_44C27D: posição += vel
// (16.16, fração em 16 bits com carry), [st+0x64]--, e o giro: se [st+0x68]
// > 0, [st+0x68]-- e rotação += [st+0x60].
static void applyStepTick(UnitRecord& u) {
    const int totalW = g_gameMap.width() * 32;
    uint32_t fx = static_cast<uint32_t>(u.fracX) + static_cast<uint16_t>(u.velX & 0xFFFF);
    u.fracX = static_cast<uint16_t>(fx);
    int px = u.pixelX + (u.velX >> 16) + static_cast<int>(fx >> 16);
    u.pixelX = ((px % totalW) + totalW) % totalW;            // and dword_54EBBC (wrap)
    uint32_t fy = static_cast<uint32_t>(u.fracY) + static_cast<uint16_t>(u.velY & 0xFFFF);
    u.fracY = static_cast<uint16_t>(fy);
    u.pixelY += (u.velY >> 16) + static_cast<int>(fy >> 16);
    --u.stepTicks;
    if (u.turnTicks) {
        --u.turnTicks;
        u.rotation = static_cast<uint8_t>(u.rotation + u.turnStep);
    }
}

// Monta o passo na direção `dir` — sub_44B650. Retorna false se o passo não
// pode ser dado (a rota é descartada e recalculada).
static bool setupStep(int id, int dir) {
    UnitRecord& u = g_units[id];
    const int mask = g_gameMap.width() - 1;
    const int curX = u.pixelX >> 5, curY = u.pixelY >> 5;
    const int nx = (curX + kDir8[dir].dx) & mask;
    const int ny = curY + kDir8[dir].dy;
    // Limites jogáveis dword_54EBD4..E0. Em x o engine guarda o mapa deslocado
    // de 32 tiles (sub_4353F0: x1=32, x2=W+31; mapas >=512 de largura: sem
    // limite) — nas coordenadas do port (0..W-1) isso cobre a largura toda.
    const MAP_RECT& clip = g_gameMap.clipRect();
    if (ny < clip.y1 || ny > clip.y2) return false;

    const VehPlayerStats* vs = vehicleSheet(u.type, u.owner & 0x0F);

    // Giro: diff = (dir<<5) - rotação (byte com sinal). Passo de giro = Turn_Rate
    // ([desc+0x30+p*0x44]); [st+0x68] = |diff|/passo (1 se |diff| <= passo).
    const uint8_t target = static_cast<uint8_t>(dir << 5);
    const int8_t diff = static_cast<int8_t>(target - u.rotation);
    if (diff != 0) {
        int rate = vs ? vs->turnRate : 0x20;      // (dword_56DE68 força 0x20)
        if (rate <= 0) rate = 0x20;
        int ad = diff < 0 ? -diff : diff;
        if (ad <= rate) { rate = ad; u.turnTicks = 1; }
        else u.turnTicks = ad / rate;
        u.turnStep = static_cast<int8_t>(diff > 0 ? rate : -rate);
        if (ad > 0x10) {                         // só gira: sem deslocamento
            u.velX = u.velY = 0;
            u.stepTicks = u.turnTicks;
            return true;
        }
    } else {
        u.turnStep = 0; u.turnTicks = 0;
    }

    // Célula de destino: ocupada por outra unidade → o original desvia
    // (sub_44BD60/sub_44BBA0); aqui a rota é recalculada pelo pathfinder.
    pfSetContext(id);
    if (!pfCanEnter(nx, ny)) return false;
    const int srcCT = g_gameMap.getCellType(LOCATION{curX, curY});
    const int dstCT = g_gameMap.getCellType(LOCATION{nx, ny});
    const int dstCost = terrainMoveCost(dstCT, u.moveClass);
    if (dstCost == 0) return false;                             // loc_44B9B3

    // Transfere a ocupação para a célula de destino já no início do passo.
    g_gameMap.clearUnitOccupancy(curX, curY);
    g_gameMap.setUnitOccupancy(nx, ny, id);
    u.fracX = u.fracY = 0x8000;
    u.stepDestX = nx << 5;
    u.stepDestY = ny << 5;

    // Duração (ticks) = Move_Speed (x sqrt2 = 724/512 na diagonal)
    //                   * (custo(origem) + custo(destino)) >> 10.
    int ms = vs ? vs->moveSpeed : 1;
    if (dir & 1) {
        int t = ms * 724;
        ms = (t + ((t >> 31) & 0x1FF)) >> 9;
    }
    int ticks = (ms * (terrainMoveCost(srcCT, u.moveClass) + dstCost)) >> 10;
    // + escuridão: (byte_54EC14[coluna - dword_54729C] * ticks) >> 5 — o mapa de
    //   luz (dia/noite) ainda não foi portado; à luz do dia o termo não pesa.
    // + dano: ticks += dano*ticks / HPmáx.
    if (vs && vs->hitPoints > 0) ticks += (u.damage * ticks) / vs->hitPoints;
    if (ticks < 2) ticks = 2;
    u.stepTicks = ticks;

    // Velocidade 16.16: 32 px em `ticks` ticks → 0x200000/ticks + 1.
    const int v = 0x200000 / ticks + 1;
    u.velX = (dir == 0 || dir == 1 || dir == 7) ? v : (dir >= 3 && dir <= 5) ? -v : 0;
    u.velY = (dir >= 5) ? -v : (dir >= 1 && dir <= 3) ? v : 0;
    u.curDir = static_cast<uint8_t>(dir);
    return true;
}

// sub_44C2F0: objetivo da busca p/ o waypoint atual [ordem+8]. O último usa o
// modo/distância do bloco ([ordem+0x3C]/[+0x40]); os intermediários usam os
// bits 29-31 do ponto (0 = célula exata; n>0 = modo 1, chegar a até n tiles).
static void waypointGoal(const OrderBlock& ob, int& x, int& y, int& mode, int& dist) {
    const uint32_t wp = ob.waypoints[ob.curWaypoint];
    x = unpackLocX(wp); y = unpackLocY(wp);
    if (ob.curWaypoint == ob.countMinus1) { mode = ob.multiFlag; dist = ob.field40; }
    else if (wp & 0xE0000000u) { mode = 1; dist = static_cast<int>(wp >> 29); }
    else { mode = 0; dist = 0; }
}

// sub_44BEC0: pula os waypoints (antes do último) que já estão a até n tiles
// (n = bits 29-31; distância de Chebyshev em pixels/32, x com wrap) da unidade.
static bool waypointAdvance(UnitRecord& u) {
    OrderBlock& ob = u.order;
    bool advanced = false;
    const int W = g_gameMap.ok() ? g_gameMap.width() * 32 : 0;
    while (ob.countMinus1 > ob.curWaypoint) {
        const uint32_t wp = ob.waypoints[ob.curWaypoint];
        // pixelX/Y do port = canto do tile (o original guarda o centro, +16).
        int dx = u.pixelX - static_cast<int>(wp & 0x7FE0u);
        if (W) { if (dx < -W / 2) dx += W; else if (dx > W / 2) dx -= W; }
        const int dy = u.pixelY - static_cast<int>((wp & 0x1FF00000u) >> 15);
        const int d = std::max(std::abs(dx) >> 5, std::abs(dy) >> 5);
        if (d > static_cast<int>(wp >> 29)) break;
        ++ob.curWaypoint;
        advanced = true;
    }
    return advanced;
}

static void stepUnit(int id) {
    UnitRecord& u = g_units[id];
    if (u.id == -1 || (u.flags & UNIT_DEAD)) return;
    if (!(u.flags & UNIT_VEHICLE)) return;
    // Morrendo (ação 0x2F, sub-estado 6): o movimento para onde está.
    if (u.action == ACT_DIE) return;

    // sub_44C0B0: passo em curso? aplica 1 tick e sai (termina mesmo sem ordem).
    if (u.stepTicks > 0) { applyStepTick(u); return; }
    // Fim de um passo COM deslocamento: avança o cursor da rota e encaixa a
    // posição no destino ([st+0x78]++, [+0x14/+0x18] = [st+0x48/+0x4C]).
    if (u.velX || u.velY) {
        ++u.routeIdx;
        u.pixelX = u.stepDestX; u.pixelY = u.stepDestY;
        u.velX = u.velY = 0;
    }

    // Ações de movimento: 2 (mover), 0x26 (ficar parado: anda até o ponto e
    // segura a posição), 0x28 (patrulhar) — sub_43ACA0 inicia todas em
    // loc_43ADAD — e a aproximação de atacar/guardar (0x24/0x25).
    const bool moving = u.action == CMD_MOVE || u.action == 0x26 || u.action == 0x24 ||
                        u.action == 0x25 || u.action == 0x27 || u.action == 0x28 ||
                        u.action == 6;   // 6: ConVec indo à obra; 0x27: rota de minério
    if (u.field20 != 1 || !moving || !u.hasOrder || u.order.waypoints.empty()) {
        // Parado: sai do sub-estado de movimento (1). Os demais sub-estados
        // (espera 6, troca na doca 0x0D...) são do laço da unidade (sub_43DE50).
        if (u.field22 == 1) u.field22 = 0;
        return;
    }

    const int curX = u.pixelX >> 5, curY = u.pixelY >> 5;
    OrderBlock& ob = u.order;
    int destX = 0, destY = 0, mode = 0, dist = 0;
    // Laço de sub_44C0B0 (loc_44C114): objetivo = waypoint [ordem+8]; chegou a
    // um ponto intermediário → avança (sub_44BEC0); sem rota p/ um ponto
    // intermediário → pula-o ([ordem+8]++). (Guarda contra laço infinito.)
    for (int guard = 0;; ++guard) {
        if (ob.curWaypoint > ob.countMinus1) ob.curWaypoint = ob.countMinus1;
        waypointGoal(ob, destX, destY, mode, dist);      // sub_44C2F0
        pfSetContext(id);
        pfSetGoal(mode, destX, destY, dist);
        if (pfMoveCost(curX, curY) < 0) {                // sub_440550: chegou ao objetivo
            if (ob.curWaypoint == ob.countMinus1) {      // loc_44C1C0: fim da lista
                u.field20 = 0; u.field22 = 0;
                u.field23 = 1;                           // [+0x23] = sub-estado anterior (movimento)
                u.routeDirs.clear(); u.routeIdx = 0;
                // Patrulhar: a ordem fica; o tick da ação (case 40) recomeça.
                if (u.action == 0x28 || u.action == 0x27) return;   // (0x27: case 39)
                // Mover (casos 2-4 de jpt_43B312) zera a ação; 0x26 não tem tratamento
                // por tick (def_43B312): a ação fica 0x26 (posição mantida).
                // Atacar/guardar (0x24/0x25) seguem no tick da ação (jpt_43B312 casos 36/37).
                if (u.action == CMD_MOVE) u.action = 0;
                u.hasOrder = false; u.order = OrderBlock{};
                return;
            }
            waypointAdvance(u);                          // sub_44BEC0
            u.routeDirs.clear(); u.routeIdx = 0;
            if (guard < 64) continue;
            u.field22 = 0; return;
        }

        // (Re)computa a rota se necessário (rota real do pathfinder).
        if (u.routeIdx >= static_cast<int>(u.routeDirs.size())) {
            SearchContext sc; sc.unitId = id;
            sc.startX = curX; sc.startY = curY; sc.destX = destX; sc.destY = destY;
            int dirs[600]; int reached = 0;
            int n = pfSearchRoute(sc, dirs, 600, &reached);
            u.routeDirs.assign(dirs, dirs + n);
            u.routeIdx = 0;
            if (n == 0) {
                // loc_44C16D: sem rota p/ um ponto intermediário → o próximo.
                if (ob.curWaypoint < ob.countMinus1 && guard < 64) { ++ob.curWaypoint; continue; }
                u.field22 = 0; return;                    // sem rota: parado, não anima
            }
        }
        break;
    }

    pfSetGoal(mode, destX, destY, dist);
    if (!setupStep(id, u.routeDirs[u.routeIdx] & 7)) {
        u.routeDirs.clear(); u.routeIdx = 0; u.field22 = 0;
        return;
    }
    // Estado "em movimento" — é o que faz o sprite animar. A máquina de ações do
    // original (sub_43ACA0) põe [+0x22]=1 ao executar um passo de movimento
    // (loc_43AD6A / loc_43ADAD) e zera ao parar (loc_43AD5D).
    u.field22 = 1;
    // sub_44C0B0: montado o passo (sub_44B650 retornou 1), o MESMO tick já
    // aplica o deslocamento (jnz loc_44C27D) — sem tick parado entre células.
    applyStepTick(u);
}

void tickMovement() {
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i)
        stepUnit(i);
}

} // namespace op2
