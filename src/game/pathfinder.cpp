#include "pathfinder.h"

#include "game_map.h"
#include "terrain_cost.h"
#include "unit.h"
#include "player.h"

namespace op2 {

PfContext g_pf;
std::vector<PfContext> g_pfStack;

static constexpr int kSentinel = static_cast<int>(0x80000000);

// ---- Pilha / contexto -------------------------------------------------------

void pfResetContext() {               // sub_438100
    g_pf.unitId = 0;
    g_pf.targetX = g_pf.targetY = kSentinel;
    g_pf.target2X = g_pf.target2Y = kSentinel;
}

void pfInitStack() {                   // sub_438130
    g_pfStack.clear();
}

void pfPush() {                        // sub_438180: empilha o ctx atual e reinicia alvos
    g_pfStack.push_back(g_pf);
    g_pf.unitId = 0;
    g_pf.targetX = g_pf.targetY = kSentinel;
    g_pf.target2X = g_pf.target2Y = kSentinel;
}

void pfPop() {                         // sub_438200: restaura o ctx do topo
    if (g_pfStack.empty()) { g_pf = PfContext{}; return; }
    g_pf = g_pfStack.back();
    g_pfStack.pop_back();
    if (g_pf.unitId != 0) {            // revalida classe/tubo do ctx restaurado
        pfSetContext(g_pf.unitId);
    }
}

void pfSetContext(int unitId) {        // sub_438260
    g_pf.unitId = unitId;
    if (unitId <= 0 || unitId >= static_cast<int>(g_units.size())) {
        g_pf.moveClass = 0; g_pf.tubeCost = 0; return;
    }
    g_pf.moveClass = g_units[unitId].moveClass;   // [objUnit+0x1E8]
    g_pf.tubeCost  = pfUnitCanUseTube(unitId);     // sub_44D4E0 → dword_4E0C08
}

void pfSetTarget(int x, int y)  { g_pf.targetX = x;  g_pf.targetY = y;  }   // sub_438290
void pfSetTarget2(int x, int y) { g_pf.target2X = x; g_pf.target2Y = y; }   // sub_4382A0
void pfSetRadius(int r)         { g_pf.radius2 = r * r; }                    // sub_4382B0

// ---- Helpers de ocupação ----------------------------------------------------

bool pfPlayersAllied(int a, int b) {   // sub_490C50: bit a no allyMask de b
    if (b < 0 || b >= kMaxPlayers) return false;
    return (g_playerData[b].allyMask & (1u << (a & 0x1F))) != 0;
}

int pfUnitCanUseTube(int unitId) {     // sub_44D4E0
    if (unitId <= 0 || unitId >= static_cast<int>(g_units.size())) return 0;
    const UnitRecord& u = g_units[unitId];
    int owner = u.owner & 0x0F;
    // dword_56E304[owner*777] = campo do jogador em +0x28 (0 = pode usar parede/tubo).
    // Modelado como isHuman (refinar quando o struct completo do jogador for mapeado).
    if (owner >= 0 && owner < kMaxPlayers && g_playerData[owner].isHuman != 0) return 0;
    if (!(u.flags & 0x10)) return 0;               // flag +0x44 bit4
    uint16_t cargo = u.cargo;                       // +0x24
    if (cargo == 0x3D || cargo == 0x3E) return 1;
    return 0;
}

int pfOccupantPassable(int occId, int searcherId) {  // sub_44BD60
    if (occId <= 0 || occId >= static_cast<int>(g_units.size())) return 1;
    const UnitRecord& o = g_units[occId];
    if (o.id == -1) return 1;                        // [+4]==-1 (morto) → atravessável
    uint32_t f = o.flags;                            // [+0x44]
    if (f & UNIT_DEAD) return 1;                      // 0x20000 → atravessável
    if (!(f & UNIT_VEHICLE)) return 0;               // !bit2(=0x2? bit1) prédio → bloqueia
    if (f & 0xC0000) return 0;                        // flags 18/19 → bloqueia

    int occOwner = o.owner & 0x0F;
    int searcherOwner = 0;
    if (searcherId > 0 && searcherId < static_cast<int>(g_units.size()))
        searcherOwner = g_units[searcherId].owner & 0x0F;

    // Aliança: sub_490C50(cmdObj[occOwner], searcherOwner).
    bool allied = pfPlayersAllied(occOwner, searcherOwner);
    if (!allied) {
        if (!(f & 0x10)) return 0;                   // não-aliado e sem flag 0x10 → bloqueia
    }

    uint8_t action = o.action;                       // [+0x21]
    switch (action) {                                // ações ocupadas → bloqueia
        case 0x27: case 0x28: case 3: case 1:
        case 0x26: case 0x1F: case 0x0F:
            return 0;
        case 6:
            if (o.field22 == 0) return 0;
            break;
        default: break;
    }
    uint8_t sub = o.field22;                          // [+0x22]
    if (sub == 0x0A) return 0;
    if (f & 0x10) {                                   // flag 0x10 → exige field4E == 0x7FFF
        if (o.field4E != 0x7FFF) return 0;
    }
    return (sub != 8) ? 1 : 0;                        // bloqueia só se sub-estado 8
}

// ---- Primitivas de célula ---------------------------------------------------

int pfCanEnter(int x, int y) {         // sub_4382C0
    if (y < 0 || y >= g_gameMap.height()) return 0;
    uint32_t cell = g_gameMap.rawCell(x, y);

    if (cell & GameMap::CELL_WALLBITS) {             // 0xA8000000
        if (cell & GameMap::CELL_HARDBLOCK) return 0; // 0x28000000
        int ct = static_cast<int>(cell & 0x1F);
        if (ct >= GameMap::TUBE_CELLTYPE_LO && ct <= GameMap::TUBE_CELLTYPE_HI)
            return g_pf.tubeCost;                     // dword_4E0C08
        return 0;
    }

    int ct = static_cast<int>(cell & 0x1F);
    if (terrainMoveCost(ct, g_pf.moveClass) == 0) return 0;  // custo 0 = impassável

    uint32_t occ = cell & GameMap::CELL_OCCUPANCY;    // 0x7FF0000
    if (occ == 0) return 1;
    int occId = static_cast<int>(occ >> GameMap::CELL_OCC_SHIFT);
    if (occId == g_pf.unitId) return 1;               // própria unidade
    return pfOccupantPassable(occId, g_pf.unitId) ? 1 : 0;
}

// Tabela de direções dword_4D5338 (verbatim do asm @326542).
const DirDelta kDir8[8] = {
    { 1,  0},  // 0 E
    { 1,  1},  // 1 SE
    { 0,  1},  // 2 S
    {-1,  1},  // 3 SW
    {-1,  0},  // 4 W
    {-1, -1},  // 5 NW
    { 0, -1},  // 6 N
    { 1, -1},  // 7 NE
};

int pfDirFromDelta(int dx, int dy) {       // sub_45C540 (ecx=dx, edx=dy)
    if (dx == 0) {
        return (dy < 0) ? 6 : 2;           // N : S  (y para baixo)
    }
    // razão = (dy<<8)/dx  (idiv, trunca p/ zero como o asm)
    int ratio = (dy << 8) / dx;
    if (ratio >= 0x26A) {                   // loc_45C57C (quase vertical)
        return (dx < 0) ? 6 : 2;            //   dx<0 → N : S
    }
    if (ratio <= -0x26A) {                   // loc_45C58C
        return (dx >= 0) ? 6 : 2;           //   dx>=0 → N : S
    }
    if (ratio >= 0x6A) {                     // loc_45C596 (diagonal)
        return (dx < 0) ? 5 : 1;           //   dx<0 → NW : SE
    }
    if (ratio <= -0x6A) {                    // loc_45C5A6
        return (dx < 0) ? 3 : 7;           //   dx<0 → SW : NE
    }
    return (dx < 0) ? 4 : 0;                 // quase horizontal: W : E
}

int pfSmoothPath(int* dirs, int count) {   // sub_43FC50 (ecx=dirs, edx=count)
    int edx = count - 1;                     // último índice
    if (edx < 1) return edx + 1;
    for (;;) {                               // loc_43FC58
        int ebx = dirs[edx];                 // direção atual
        int prev = dirs[edx - 1];            // direção anterior
        int esi = (ebx - prev) & 7;          // diferença
        bool adjusted = false;
        if (prev & 1) {                      // anterior = diagonal (ímpar)
            if (esi == 3)      { edx--; ebx -= 2; adjusted = true; }
            else if (esi == 4) { edx -= 2; }
            else if (esi == 5) { edx--; ebx += 2; adjusted = true; }
            else break;                      // {0,1,2,6,7} → fim
        } else {                             // anterior = cardinal (par)
            if (esi == 2 || esi == 3) { edx--; ebx -= 1; adjusted = true; }
            else if (esi == 4)        { edx -= 2; }
            else if (esi == 5 || esi == 6) { edx--; ebx += 1; adjusted = true; }
            else break;                      // {0,1,7} → fim
        }
        if (adjusted) {                      // loc_43FCBA
            ebx &= 7;
            dirs[edx] = ebx;
        }
        if (edx < 1) break;                  // loc_43FCC0
    }
    return edx + 1;
}

StepChoice pfChooseStep(int unitId, int currentDir, int avoidDir) {
    StepChoice best;
    if (unitId <= 0 || unitId >= static_cast<int>(g_units.size())) return best;
    const UnitRecord& u = g_units[unitId];

    int back = (currentDir + 4) & 7;       // var_2C: direção de trás (não voltar)
    int curX = u.pixelX >> 5;              // [+0x14] >> 5
    int curY = u.pixelY >> 5;              // [+0x18] >> 5

    pfPush();                              // sub_438180
    pfSetContext(unitId);                  // sub_438260

    int bestCost = PF_BLOCKED;             // var_28
    int ebx = back;
    for (;;) {
        ebx = (ebx + 1) & 7;               // próxima direção
        if (ebx == back) break;            // tentou todas menos a de trás
        int newX = (curX + kDir8[ebx].dx) & (g_gameMap.width() - 1);  // wrap em X
        int newY = curY + kDir8[ebx].dy;
        if (newY <= 0) newY = 0;
        if (newY >= g_gameMap.height()) newY = g_gameMap.height() - 1;

        int cost = pfMoveCost(newX, newY); // sub_438390
        if (ebx == avoidDir) {             // breadcrumb → penaliza (3*cost - sign) >> 1
            int32_t t = static_cast<int32_t>(static_cast<uint32_t>(cost) * 3u);  // wrap 32-bit como o asm
            t -= (t < 0 ? -1 : 0);         // sub eax, edx (edx = sign)
            cost = t >> 1;                 // sar eax, 1 (aritmético)
        }
        if (cost < bestCost) {
            bestCost = cost;
            best.dir = ebx;
            best.x = newX;
            best.y = newY;
        }
    }

    pfPop();                               // sub_438200

    if (bestCost != PF_BLOCKED) {
        best.found = true;
        best.cost = bestCost;
    }
    return best;
}

int pfMoveCost(int x, int y) {         // sub_438390
    if (y < 0 || y >= g_gameMap.height()) return PF_BLOCKED;
    uint32_t cell = g_gameMap.rawCell(x, y);

    int base;
    if (cell & GameMap::CELL_WALLBITS) {              // 0xA8000000
        int gate;
        if (cell & GameMap::CELL_HARDBLOCK) {         // 0x28000000
            gate = 0;
        } else {
            int ct = static_cast<int>(cell & 0x1F);
            gate = (ct >= GameMap::TUBE_CELLTYPE_LO && ct <= GameMap::TUBE_CELLTYPE_HI)
                   ? g_pf.tubeCost : 0;
        }
        if (gate == 0) return PF_BLOCKED;
        base = 0x8000;                                // custo-base fixo de parede/tubo
    } else {
        int ct = static_cast<int>(cell & 0x1F);
        base = terrainMoveCost(ct, g_pf.moveClass);
        if (base == 0) return PF_BLOCKED;
    }

    // Célula-alvo → custo negativo (prioridade).
    if (x == g_pf.targetX && y == g_pf.targetY) base = -base;

    uint32_t occ = cell & GameMap::CELL_OCCUPANCY;
    if (occ == 0) return base;
    int occId = static_cast<int>(occ >> GameMap::CELL_OCC_SHIFT);
    if (occId == g_pf.unitId) return base;            // própria unidade

    if (occId <= 0 || occId >= static_cast<int>(g_units.size())) return base;
    const UnitRecord& o = g_units[occId];
    if (o.id == -1) return base;                      // morto
    if (o.flags & UNIT_DEAD) return base;             // 0x20000
    if (!(o.flags & UNIT_VEHICLE)) return PF_BLOCKED; // prédio → bloqueia

    if (!pfOccupantPassable(occId, g_pf.unitId)) return PF_BLOCKED;  // inimigo/ocupado
    if (o.field22 == 0) return base + base;           // veículo parado → ×2
    return base * 4;                                  // veículo movendo → ×4
}

} // namespace op2
