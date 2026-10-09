#include "pathfinder.h"
#include "weapon.h"

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
    g_pf.goalMode = 0;
    g_pf.targetX = g_pf.targetY = kSentinel;
    g_pf.target2X = g_pf.target2Y = kSentinel;
}

void pfInitStack() {                   // sub_438130
    g_pfStack.clear();
}

void pfPush() {                        // sub_438180: empilha o ctx atual e reinicia alvos
    g_pfStack.push_back(g_pf);
    g_pf.unitId = 0;
    g_pf.goalMode = 0;
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
void pfSetGoal(int mode, int x, int y, int dist) {
    g_pf.goalMode = mode;
    pfSetTarget(x, y);
    if (mode == 1 || mode == 2) { pfSetTarget2(x, y); pfSetRadius(dist); }
}

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

int pfWallFollow(int& dir, int& x, int& y, int turnSign) {  // sub_43FBB0
    int rev = 8 - turnSign;                 // var_4: passo de rotação reverso
    int mask = g_gameMap.width() - 1;
    for (int guard = 0; guard < 8; ++guard) {   // no original é ilimitado; guard = 1 volta
        int d = dir & 7;
        int nx = (x + kDir8[d].dx) & mask;
        int ny = y + kDir8[d].dy;
        if (pfCanEnter(nx, ny)) {           // dword_550700
            x = nx;
            y = ny;
            dir = (d + ((d & 1) ? turnSign * 2 : turnSign)) & 7;  // ajuste (×2 se diagonal)
            return d;                        // direção do passo (eax no asm)
        }
        dir = (d + rev) & 7;                 // rotaciona
    }
    return dir & 7;
}

// Distância horizontal com wrap em X, mapeada p/ [-metade, +metade] (loc_43F609
// do asm): diff = dx; se diff < -half → +width; se diff > half → -((-diff)&mask).
static int pfWrapDX(int diff) {
    int mask = g_gameMap.width() - 1;      // dword_54EBC4
    int w    = g_gameMap.width();           // dword_54EBC8
    int half = mask >> 1;
    if (-half > diff) diff += w;
    else if (half < diff) diff = -(((-diff)) & mask);
    return diff;
}

int pfOctDistToTarget(int x, int y) {       // sub_43FCD0
    int dx = pfWrapDX(g_pf.targetX - x);
    int adx = (dx < 0 ? -dx : dx) << 16;
    int dy = g_pf.targetY - y;
    int ady = (dy < 0 ? -dy : dy) << 16;
    int mx = adx, mn = ady;
    if (mn > mx) { int t = mx; mx = mn; mn = t; }   // mx = maior, mn = menor
    return mx + (mn >> 1);                           // max + min/2
}

// dword_4D0510: custo de passo cardinal (dir par) / diagonal (dir ímpar).
static const int kStepCost[2] = { 0x20, 0x2D };

// sub_43FF20 @106667 — avalia truncamentos do caminho no sentido FORWARD. Para
// cada prefixo i, anexa dir[i] ao buffer suavizado (pfSmoothPath), acumula o custo
// de passo e soma a distância octogonal de pos[i] ao alvo; guarda o (custo,índice)
// mínimo. Para se a distância chega a 0 (alcançou o alvo).
static void evalForward(const int* dir, int count, const int* px, const int* py,
                        int& outCost, int& outIdx) {
    outCost = 0x7FFFFFFF;
    outIdx = 0;
    static int sm[640];
    static int cum[640];
    int L = 0;
    for (int i = 0; i < count; ++i) {
        sm[L] = dir[i];
        L = pfSmoothPath(sm, L + 1);         // sub_43FC50
        int ebx = L - 1;
        cum[ebx] = kStepCost[sm[ebx] & 1] + (ebx > 0 ? cum[ebx - 1] : 0);
        int dist = pfOctDistToTarget(px[i], py[i]);   // sub_43FCD0
        int total = cum[ebx] + dist;
        if (total < outCost) { outCost = total; outIdx = i; }
        if (dist == 0) break;
    }
}

// sub_440080 @106817 — igual, mas no sentido REVERSE: percorre dir[] do fim para o
// começo com cada direção invertida ((d+4)&7) e pos[] de trás para frente.
static void evalReverse(const int* dir, int count, const int* px, const int* py,
                        int& outCost, int& outIdx) {
    outCost = 0x7FFFFFFF;
    outIdx = 0;
    static int sm[640];
    static int cum[640];
    int L = 0;
    // edi = count-1 .. 1 ; pos indexado de [count-2] para trás (como o asm).
    for (int i = count - 1, p = count - 2; i > 0; --i, --p) {
        sm[L] = (dir[i] + 4) & 7;            // direção invertida
        L = pfSmoothPath(sm, L + 1);
        int ebx = L - 1;
        cum[ebx] = kStepCost[sm[ebx] & 1] + (ebx > 0 ? cum[ebx - 1] : 0);
        int dist = (p >= 0) ? pfOctDistToTarget(px[p], py[p]) : 0;
        int total = cum[ebx] + dist;
        if (total < outCost) { outCost = total; outIdx = i; }
        if (dist == 0) break;
    }
}

// Emenda os primeiros (idx+1) passos de um contorno à rota de saída, suavizando
// (loc_43F9DC do asm). idx < 0 não emenda nada.
static int appendContour(const int* dir, int idx, int* out, int pathLen, int maxDirs) {
    if (idx < 0) return pathLen;
    for (int k = 0; k <= idx; ++k) {
        if (pathLen >= maxDirs) break;
        out[pathLen++] = dir[k];
        pathLen = pfSmoothPath(out, pathLen);
    }
    return pathLen;
}

int pfSearchRoute(const SearchContext& ctx, int* outDirs, int maxDirs, int* reached) {
    // sub_43F5B0 — busca de rota (seguir-parede bidirecional).
    int curX = ctx.startX;
    int curY = ctx.startY;
    int pathLen = 0;
    int failedOnce = 0;                                // var_3858
    int mask = g_gameMap.width() - 1;
    const MAP_RECT& clip = g_gameMap.clipRect();
    int cx1 = clip.x1, cy1 = clip.y1, cx2 = clip.x2, cy2 = clip.y2;  // 54EBD4/D8/DC/E0
    static int dirA[640], posAx[640], posAy[640];
    static int dirB[640], posBx[640], posBy[640];

    for (;;) {
        // --- FASE A: reta rumo ao alvo (loc_43F5EB) ---
        int blockedDir = 0;
        bool blocked = false;
        for (;;) {
            if (curX == ctx.destX && curY == ctx.destY) {
                if (reached) *reached = 1;
                return pathLen;
            }
            if (pathLen >= maxDirs) {
                if (reached) *reached = 0;
                return pathLen;
            }
            int dx = pfWrapDX(ctx.destX - curX);
            int dy = ctx.destY - curY;
            int dir = pfDirFromDelta(dx, dy);           // sub_45C540
            int nx = (curX + kDir8[dir].dx) & mask;
            int ny = curY + kDir8[dir].dy;
            if (pfCanEnter(nx, ny)) {                     // passo reto possível
                outDirs[pathLen++] = dir;
                pathLen = pfSmoothPath(outDirs, pathLen); // sub_43FC50
                curX = nx; curY = ny;
                continue;
            }
            blocked = true; blockedDir = dir; break;      // obstáculo → FASE B
        }
        (void)blocked;

        // --- FASE B: contorno de obstáculo (loc_43F6C6) ---
        if (failedOnce) {                                 // só um desvio por chamada
            if (reached) *reached = 0;
            return pathLen;
        }
        int budget = maxDirs - pathLen;                   // var_3884

        // Contorno A (turnSign = 7): entrada contada antes do teste de clip.
        int cntA = 0;
        { int wd = (blockedDir + 1) & 7, wx = curX, wy = curY;
          while (!(budget < cntA)) {                       // cntA <= budget
              int sd = pfWallFollow(wd, wx, wy, 7);
              dirA[cntA] = sd; posAx[cntA] = wx; posAy[cntA] = wy; ++cntA;
              if (wy == cy1 && wd == 7) break;
              if (wy == cy2 && wd == 3) break;
              if (wx == cx1 && wd == 5) break;
              if (wx == cx2 && wd == 1) break;
          }
        }
        int useA = (budget < cntA) ? budget : cntA;        // min(budget, cntA)

        // Contorno B (turnSign = 1): dir gravado antes do teste; só conta se não quebrar.
        int cntB = 0;
        if (budget > 0) { int wd = (blockedDir - 1) & 7, wx = curX, wy = curY;
          while (cntB < budget) {
              int sd = pfWallFollow(wd, wx, wy, 1);
              dirB[cntB] = sd;
              if (wy == cy1 && wd == 5) break;
              if (wy == cy2 && wd == 1) break;
              if (wx == cx1 && wd == 3) break;
              if (wx == cx2 && wd == 7) break;
              posBx[cntB] = wx; posBy[cntB] = wy; ++cntB;
          }
        }

        // Avalia os dois contornos e a distância direta.
        int costA, idxA, costB, idxB;
        evalForward(dirA, useA, posAx, posAy, costA, idxA);   // sub_43FF20
        evalForward(dirB, cntB, posBx, posBy, costB, idxB);
        int distDirect = pfOctDistToTarget(curX, curY);       // sub_43FCD0

        // Seleção: menor custo; empate → prefere A se idxA < idxB.
        bool chooseB;
        if (costB > costA) chooseB = false;
        else if (costB < costA) chooseB = true;
        else chooseB = !(idxA < idxB);

        int endX = curX, endY = curY;
        if (chooseB) {
            if (distDirect > costB) {                          // usa contorno B
                endX = posBx[idxB]; endY = posBy[idxB];
                pathLen = appendContour(dirB, idxB, outDirs, pathLen, maxDirs);
            }
        } else {
            if (distDirect > costA) {                          // usa contorno A
                endX = posAx[idxA]; endY = posAy[idxA];
                pathLen = appendContour(dirA, idxA, outDirs, pathLen, maxDirs);
            }
        }

        // loc_43FB2E: sem progresso → encerra; senão marca failedOnce e segue.
        if (curX == endX && curY == endY) {
            if (reached) *reached = (curX == ctx.destX && curY == ctx.destY) ? 1 : 0;
            return pathLen;
        }
        failedOnce = 1;                                        // desvio normal (eax==0)
        curX = endX; curY = endY;
    }
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
    bool goal = x == g_pf.targetX && y == g_pf.targetY;
    if (!goal && (g_pf.goalMode == 1 || g_pf.goalMode == 2)) {   // sub_4384D0 / sub_438660
        const int dx = pfWrapDX(g_pf.target2X - x), dy = g_pf.target2Y - y;
        if (dx * dx + dy * dy <= g_pf.radius2) {
            if (g_pf.goalMode == 1 || (dx == 0 && dy == 0)) goal = true;
            else goal = mapLineOfSight(x, y, g_pf.target2X, g_pf.target2Y) != 0;   // sub_436EA0
        }
    }
    if (goal) base = -base;

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
