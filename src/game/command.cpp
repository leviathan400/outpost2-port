#include "command.h"

#include "unit.h"
#include "game_map.h"
#include "pathfinder.h"

namespace op2 {

PathScratch g_pathScratch;

// Avança a espiral global até a próxima célula enterável e devolve (x,y) nela
// (porta a máquina de estado de sub_4230F0 @loc_4231FE): passo na direção atual
// (E/N/O/S com y clampeado), perna cresce a cada 2 viradas, usa pfCanEnter com o
// contexto da unidade que busca. Guard contra laço infinito (mapa sem célula livre).
static bool spiralNext(int& outX, int& outY) {
    int mask = g_gameMap.width() - 1;
    int H = g_gameMap.height();
    PathScratch& s = g_pathScratch;
    for (int guard = 0; guard < 1 << 16; ++guard) {
        switch (s.state & 3) {               // passo na direção atual
            case 0: s.x += 1; break;                                   // Leste
            case 1: s.y -= 1; if (s.y <= 0) s.y = 0; break;            // Norte (clamp >=0)
            case 2: s.x -= 1; break;                                   // Oeste
            case 3: s.y += 1; if (s.y >= H - 1) s.y = H - 1; break;    // Sul (clamp <=H-1)
        }
        if (--s.steps == 0) {                 // fim da perna
            s.state = (s.state + 1) & 3;
            if ((s.state & 1) == 0) ++s.leg;  // cresce a perna a cada 2 viradas (E/O)
            s.steps = s.leg;
        }
        int wx = s.x & mask;                  // x com wrap
        if (pfCanEnter(wx, s.y)) { outX = wx; outY = s.y; return true; }
    }
    return false;
}

// Reset do estado de movimento antes de aplicar nova ordem (porta sub_43A910,
// chamado pelo dispatcher antes de sub_4230F0): limpa o progresso/ordem atual.
static void resetUnitOrder(UnitRecord& u) {
    u.order.curWaypoint = 0;
    u.field3C = 0;
    u.routeDirs.clear();          // invalida a rota antiga; o tick recomputa
    u.routeIdx = 0;
    // sub_43A910 NÃO mexe no passo em curso ([st+0x64]): ele termina normalmente.
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
        // 1ª unidade: arma a espiral com o DESTINO (último waypoint); mantém a
        // célula clicada como destino dela. (loc_423191 do asm.)
        g_pathScratch.state = 0;      // dword_5472EC = 0
        g_pathScratch.leg = 1;        // dword_5472E8 = 1
        g_pathScratch.steps = 1;      // dword_5472F0 = 1
        uint32_t last = ob.waypoints.empty() ? 0u : ob.waypoints.back();
        g_pathScratch.x = unpackLocX(last);  // dword_5472F4
        g_pathScratch.y = unpackLocY(last);  // dword_5472F8
    } else {
        // Unidades seguintes: avançam a espiral até a próxima célula enterável e
        // usam-na como destino (espalha o grupo). (máquina de estado loc_4231FE,
        // que sobrescreve o destino do bloco de ordem com a célula achada.)
        pfSetContext(u.id);
        int sx, sy;
        if (spiralNext(sx, sy) && !ob.waypoints.empty())
            ob.waypoints.back() = packLocation(sx, sy);
    }
}

void processCommandPacket(int owner, const CommandPacket& pkt) {
    // Dispatcher sub_40E210 — caso MOVE (loc_40E2C0). Aplica a ordem a cada
    // unidade do packet cujo dono confere e que aceita comando.
    // Casos 2-4 e 38-40 (loc_40E2C0): ordem de movimento com a ação = tipo.
    // Portados: 2 (mover) e 0x26 (ficar parado).
    if (pkt.type != CMD_MOVE && pkt.type != 0x26) return;

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
