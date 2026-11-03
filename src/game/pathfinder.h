#pragma once

#include <cstdint>
#include <vector>

namespace op2 {

// Pathfinder do OP2 (porta o módulo sub_438100..sub_439C00+). Esta parte cobre o
// CONTEXTO de busca (globais dword_54F028/30/34/38/3C/40/44 + pilha unk_54F048/54F0A8)
// e as duas primitivas de célula: canEnter (sub_4382C0) e moveCost (sub_438390).
// O laço de busca em si entra no próximo sub-passo.

// Contexto de busca atual — equivale a dword_54F030 (ctx) + alvos/raio. No original
// o ctx é um objeto Unit com vtable cujo [+0x1E8] é a classe de movimento; aqui o
// representamos pelo id da unidade que busca (dá dono/classe/tubo).
struct PfContext {
    int unitId  = 0;                 // dword_54F030 → unidade que busca (0 = nenhum)
    int moveClass = 0;               // dword_54F028 ([unidade+0x1E8])
    int tubeCost  = 0;               // dword_4E0C08 (sub_44D4E0: 0/1, gate de parede/tubo)
    int targetX = static_cast<int>(0x80000000);  // dword_54F034
    int targetY = static_cast<int>(0x80000000);  // dword_54F038
    int target2X = static_cast<int>(0x80000000); // dword_54F03C
    int target2Y = static_cast<int>(0x80000000); // dword_54F040
    int radius2  = 0;                // dword_54F044 (raio²)
    // Objetivo da busca ([mov+0xCC], sub_4401F0 escolhe a função de custo):
    // 0 tile exato (sub_438390); 1 até o raio de target2 (sub_4384D0);
    // 2 até o raio e com linha de visada (sub_438660); 3 ao lado do alvo, com
    // parede/tubo sempre bloqueando (sub_438850 — ver pfMoveCost).
    int goalMode = 0;
};

extern PfContext g_pf;                 // contexto atual (dword_54F030...)
extern std::vector<PfContext> g_pfStack;  // pilha (unk_54F048; sp = tamanho)

// --- Pilha/contexto (sub_438100..sub_4382B0) --------------------------------
void pfResetContext();                 // sub_438100: zera ctx, alvos = 0x80000000
void pfInitStack();                    // sub_438130: zera a pilha
void pfPush();                         // sub_438180: empilha ctx atual e reinicia
void pfPop();                          // sub_438200: desempilha ctx
void pfSetContext(int unitId);         // sub_438260: define ctx (classe/tubo da unidade)
void pfSetTarget(int x, int y);        // sub_438290: dword_54F034/38
void pfSetTarget2(int x, int y);       // sub_4382A0: dword_54F03C/40
void pfSetRadius(int r);               // sub_4382B0: dword_54F044 = r*r
// sub_4401F0: arma o objetivo — modo 0 só o alvo; 1/2 também target2 = alvo e
// raio = dist (sub_4382A0/sub_4382B0).
void pfSetGoal(int mode, int x, int y, int dist);

// --- Primitivas de célula ---------------------------------------------------
// canEnter (sub_4382C0): 1 = a unidade do contexto pode entrar em (x,y); 0 = não.
int pfCanEnter(int x, int y);
// moveCost (sub_438390): custo de entrar em (x,y) (negativo = célula-alvo);
// 0x7FFFFFFF = intransponível.
int pfMoveCost(int x, int y);
constexpr int PF_BLOCKED = 0x7FFFFFFF;

// --- Direções (tabela dword_4D5338) -----------------------------------------
// 8 direções horárias a partir de Leste, y para baixo. Oposto = (d+4)&7.
struct DirDelta { int dx, dy; };
extern const DirDelta kDir8[8];

// Resultado da escolha de passo.
struct StepChoice {
    bool found = false;
    int dir = 0;      // direção escolhida (0..7)
    int x = 0, y = 0; // célula de destino do passo (tile)
    int cost = PF_BLOCKED;
};

// pfChooseStep (parte de seleção de sub_44BBA0 @122793): avalia as 8 direções
// vizinhas da unidade `unitId` (exceto a de trás = (currentDir+4)&7), calcula o
// custo de cada com pfMoveCost e escolhe a de menor custo. `avoidDir` recebe
// penalidade ×1.5 (anti-oscilação; breadcrumb do original). Faz push/set/pop do
// contexto como o original. Sem movimento possível → found=false.
StepChoice pfChooseStep(int unitId, int currentDir, int avoidDir);

// --- Blocos da busca de rota (componentes do pathfinder real) ----------------
// pfDirFromDelta (sub_45C540 @149753): direção-octante (0..7, kDir8) a partir do
// vetor delta (dx,dy). Usa a razão (dy<<8)/dx com limiares 0x6A (tan 22.5°) e
// 0x26A (tan 67.5°). Base da busca de rota (direção start→dest).
int pfDirFromDelta(int dx, int dy);

// pfSmoothPath (sub_43FC50 @106305): suaviza um array de direções `dirs[0..n-1]`
// (colapsa curvas redundantes/zigue-zagues). Edita o array no lugar e devolve o
// novo tamanho. Pós-processamento da rota encontrada.
int pfSmoothPath(int* dirs, int count);

// pfWallFollow (sub_43FBB0 @106230): primitiva de seguir-parede da busca. A partir
// de (x,y) e direção `dir`, rotaciona `dir` em passos de (8-turnSign) até a célula
// vizinha ser enterável (pfCanEnter, com o contexto atual), anda para ela e ajusta
// `dir` por turnSign (×2 se diagonal). Edita dir/x/y no lugar. Assume que existe ao
// menos uma célula enterável (como o original garante antes de chamar). Devolve a
// direção do passo efetivamente dado (como sub_43FBB0 retorna em eax).
int pfWallFollow(int& dir, int& x, int& y, int turnSign);

// pfOctDistToTarget (sub_43FCD0 @106389): distância octogonal (max+min/2, ×65536)
// de (x,y) ao destino do contexto atual (g_pf.targetX/Y), com wrap em X.
int pfOctDistToTarget(int x, int y);

// Contexto de uma busca de rota (equivale ao objeto dword_550700 do original:
// +0x0C/10 = start, +0x14/18 = dest).
struct SearchContext {
    int unitId = 0;
    int startX = 0, startY = 0;
    int destX = 0, destY = 0;
};

// pfSearchRoute (sub_43F5B0 @105666): busca de rota (seguir-parede bidirecional).
// Escreve as direções (0..7) da rota em outDirs (até maxDirs) e devolve o tamanho;
// se `reached` != null, *reached = 1 se a rota chega ao destino. Requer o contexto
// de pathfinder já armado (pfSetContext(unitId) + pfSetTarget(dest)).
// FASE A (reta rumo ao alvo) portada; FASE B (contorno de obstáculo) em progresso.
int pfSearchRoute(const SearchContext& ctx, int* outDirs, int maxDirs, int* reached);

// pfPlanRoute (sub_4401F0): monta a próxima rota da unidade em routeDirs.
// Objetivo exato (modo 0) a dist² <= 4: sub_440BA0. Com o refinamento do dono
// ([jogador+0x1B8], ou modo 3) e dist² <= 0x240: busca no corredor da reta
// (sub_440660 + sub_441250). Senão, a busca longa sub_43F5B0 enche o cache
// (routeCache, +0xD4/+0xBC/+0xC0, posição +0xC4/+0xC8), que sai em trechos de
// 64 direções (sub_441160) ou refeitos de 48 em 48 no corredor (sub_440C50).
// Devolve o "chegou" ([+0x38]).
int pfPlanRoute(int unitId, int sx, int sy, int destX, int destY, int mode, int dist);

// --- Helpers de ocupação (sub_44BD60 / sub_44D4E0 / sub_490C50) --------------
// Jogador `a` é aliado de `b`? (sub_490C50: bit a no allyMask de b.)
bool pfPlayersAllied(int a, int b);
// A unidade pode atravessar paredes/tubos especiais? (sub_44D4E0 → tubeCost.)
int  pfUnitCanUseTube(int unitId);
// O ocupante `occId` bloqueia a passagem da unidade que busca? (sub_44BD60:
// retorna !=0 = atravessável, 0 = bloqueia.)
int  pfOccupantPassable(int occId, int searcherId);

} // namespace op2
