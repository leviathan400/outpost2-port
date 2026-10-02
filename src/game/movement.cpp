#include "movement.h"

#include "unit.h"
#include "game_map.h"
#include "pathfinder.h"
#include "command.h"

namespace op2 {

static void stepUnit(int id) {
    UnitRecord& u = g_units[id];
    if (u.id == -1 || (u.flags & UNIT_DEAD)) return;
    if (!(u.flags & UNIT_VEHICLE)) return;
    if (u.field20 != 1 || u.action != CMD_MOVE) return;   // sem ordem de movimento ativa
    if (!u.hasOrder || u.order.waypoints.empty()) return;

    const int mask = g_gameMap.width() - 1;
    const int totalW = g_gameMap.width() * 32;

    // Se está deslizando entre células, avança o deslize e retorna.
    if (u.sliding) {
        u.slideProg += u.moveSpeed;
        if (u.slideProg >= 32) u.slideProg = 32;
        int px = u.slideSX + kDir8[u.slideDir].dx * u.slideProg;
        px = ((px % totalW) + totalW) % totalW;           // wrap horizontal
        u.pixelX = px;
        u.pixelY = u.slideSY + kDir8[u.slideDir].dy * u.slideProg;
        if (u.slideProg >= 32) {                           // chegou na célula
            int nx = (u.slideSX >> 5) + kDir8[u.slideDir].dx;
            int ny = (u.slideSY >> 5) + kDir8[u.slideDir].dy;
            nx &= mask;
            u.pixelX = nx << 5;
            u.pixelY = ny << 5;
            u.sliding = false;
            ++u.routeIdx;
        }
        return;
    }

    int curX = u.pixelX >> 5, curY = u.pixelY >> 5;
    int destX = unpackLocX(u.order.waypoints.back());
    int destY = unpackLocY(u.order.waypoints.back());

    if (curX == destX && curY == destY) {                 // chegou ao destino
        u.field20 = 0; u.action = 0; u.field22 = 0;
        u.hasOrder = false; u.order = OrderBlock{};
        u.routeDirs.clear(); u.routeIdx = 0;
        return;
    }

    // (Re)computa a rota se necessário (rota real do pathfinder).
    if (u.routeIdx >= static_cast<int>(u.routeDirs.size())) {
        pfSetContext(id);
        pfSetTarget(destX, destY);
        SearchContext sc; sc.unitId = id;
        sc.startX = curX; sc.startY = curY; sc.destX = destX; sc.destY = destY;
        int dirs[600]; int reached = 0;
        int n = pfSearchRoute(sc, dirs, 600, &reached);
        u.routeDirs.assign(dirs, dirs + n);
        u.routeIdx = 0;
        if (n == 0) return;                               // sem rota: espera próximo tick
    }

    int dir = u.routeDirs[u.routeIdx] & 7;
    int nx = (curX + kDir8[dir].dx) & mask;
    int ny = curY + kDir8[dir].dy;
    // Checa dinamicamente (outra unidade pode ter ocupado): recomputa se bloqueado.
    pfSetContext(id);
    pfSetTarget(destX, destY);
    if (!pfCanEnter(nx, ny)) { u.routeDirs.clear(); u.routeIdx = 0; return; }

    // Inicia o deslize: reserva a próxima célula, libera a atual.
    g_gameMap.clearUnitOccupancy(curX, curY);
    g_gameMap.setUnitOccupancy(nx, ny, id);
    u.slideDir = dir;
    u.slideProg = 0;
    u.slideSX = curX << 5;
    u.slideSY = curY << 5;
    u.sliding = true;
    u.curDir = static_cast<uint8_t>(dir);
    u.rotation = static_cast<uint8_t>((dir << 5) & 0xFF);
}

void tickMovement() {
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i)
        stepUnit(i);
}

} // namespace op2
