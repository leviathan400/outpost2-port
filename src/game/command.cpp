#include "command.h"

#include "unit.h"

namespace op2 {

PathScratch g_pathScratch;

// Reset do estado de movimento antes de aplicar nova ordem (porta sub_43A910,
// chamado pelo dispatcher antes de sub_4230F0): limpa o progresso/ordem atual.
static void resetUnitOrder(UnitRecord& u) {
    u.order.curWaypoint = 0;
    u.field3C = 0;
    u.routeDirs.clear();          // invalida a rota antiga; o tick recomputa
    u.routeIdx = 0;
    u.sliding = false;
}

// sub_4230F0 @asm 57211 — aloca/preenche o bloco de ordem (unit+0x38) e, para a
// 1ª unidade do grupo, arma o scratch global do pathfinder (deixando a busca
// para o tick). As unidades seguintes rodam a busca imediata no original
// (sub_438180/sub_438260 + máquina de estado): isso é o passo 4; aqui elas
// também adiam (no caso de 1 unidade, unitIndex é sempre 0, então o caminho
// exercitado é exatamente o do original).
static void setUnitMoveOrder(UnitRecord& u, int count,
                             const std::vector<uint32_t>& waypoints,
                             int unitIndex, int numUnits) {
    // [ebp+3Ch] = unitIndex/4 (sobrescrito depois pelo dispatcher; mantido fiel).
    u.field3C = unitIndex / 4;
    u.action = 2;       // [ebp+21h] = 2 (move)
    u.field20 = 1;      // [ebp+20h] = 1

    // Aloca o bloco de ordem se ainda não existe (unit+0x38 == null).
    if (!u.hasOrder) u.hasOrder = true;

    // Zera e preenche (0x35 dwords zerados no original; aqui os campos modelados).
    OrderBlock& ob = u.order;
    ob = OrderBlock{};
    ob.count = count;                 // [0x00]
    ob.countMinus1 = count - 1;       // [0x04]
    ob.waypoints.assign(waypoints.begin(),
                        waypoints.begin() + (count <= (int)waypoints.size()
                                             ? count : (int)waypoints.size()));  // [0x14...]
    ob.flags |= 1;                    // [0x34] |= 1

    if (numUnits > 4) {               // grupos grandes
        ob.multiFlag = 1;             // [0x3C] = 1
        ob.field40 = (numUnits + 7) / 8 + 1;  // [0x40]
    }

    if (unitIndex == 0) {
        // 1ª unidade: arma o scratch global com o DESTINO (último waypoint) e adia
        // a busca. (loc_423191 do asm.)
        g_pathScratch.state = 0;      // dword_5472EC = 0
        g_pathScratch.active = 1;     // dword_5472E8 = 1
        g_pathScratch.flag = 1;       // dword_5472F0 = 1
        uint32_t last = ob.waypoints.empty() ? 0u : ob.waypoints.back();
        g_pathScratch.targetX = unpackLocX(last);  // dword_5472F4
        g_pathScratch.targetY = unpackLocY(last);  // dword_5472F8
    }
    // unitIndex != 0: a busca imediata (sub_438180/sub_438260) entra no passo 4.
}

void processCommandPacket(int owner, const CommandPacket& pkt) {
    // Dispatcher sub_40E210 — caso MOVE (loc_40E2C0). Aplica a ordem a cada
    // unidade do packet cujo dono confere e que aceita comando.
    if (pkt.type != CMD_MOVE) return;   // outros casos: passos futuros

    const int numUnits = static_cast<int>(pkt.unitIds.size());
    for (int idx = 0; idx < numUnits; ++idx) {
        int id = pkt.unitIds[idx];
        if (id <= 0 || id >= static_cast<int>(g_units.size())) continue;
        UnitRecord& u = g_units[id];
        if (u.id == -1) continue;                        // slot morto ([esi+4]==-1)
        if ((u.owner & 0x0F) != (owner & 0x0F)) continue; // dono não confere
        // "can-command" (vtbl[0x74]): no original decide se a unidade aceita o
        // comando; p/ MOVE só veículos vivos se movem — aproximação fiel ao caso.
        if (u.flags & UNIT_DEAD) continue;
        if (!(u.flags & UNIT_VEHICLE)) continue;
        if (u.field22 == 0x0D) continue;                 // não-comandável

        resetUnitOrder(u);                               // sub_43A910
        setUnitMoveOrder(u, static_cast<int>(pkt.waypoints.size()),
                         pkt.waypoints, idx, numUnits);  // sub_4230F0

        u.action  = static_cast<uint8_t>(pkt.type);      // [esi+21h] = tipo
        u.field20 = 1;                                    // [esi+20h] = 1
        u.field3C = 0;                                    // [esi+3Ch] = 0
    }
}

} // namespace op2
