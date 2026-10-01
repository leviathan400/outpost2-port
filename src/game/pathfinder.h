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

// --- Helpers de ocupação (sub_44BD60 / sub_44D4E0 / sub_490C50) --------------
// Jogador `a` é aliado de `b`? (sub_490C50: bit a no allyMask de b.)
bool pfPlayersAllied(int a, int b);
// A unidade pode atravessar paredes/tubos especiais? (sub_44D4E0 → tubeCost.)
int  pfUnitCanUseTube(int unitId);
// O ocupante `occId` bloqueia a passagem da unidade que busca? (sub_44BD60:
// retorna !=0 = atravessável, 0 = bloqueia.)
int  pfOccupantPassable(int occId, int searcherId);

} // namespace op2
