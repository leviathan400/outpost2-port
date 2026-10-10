#pragma once

#include <string>
#include <vector>

namespace op2 {

// Comandos da interface (botões do painel) — porta das classes de comando do
// OUTPOST2.EXE (vtables off_4D4AF8..off_4D5170; instâncias globais
// dword_5648xx/5649xx). Cada classe tem:
//   vt[0] disponível p/ a seleção atual?      vt[2] descritor do botão
//   vt[1] clique no botão (comando imediato ou modo de mira)
//   vt[3] alvo escolhido no mapa (modo de mira)
//
// Botoeira do veículo (painel off_4D5B28, sub_46D540): percorre a lista dos 22
// comandos (1 unidade, off_4E2C30) ou dos 11 (várias, off_4E2C88); cada
// disponível vira um botão 3x3 na posição do descritor ([+0x1E] 1..9):
//   x = 28 + 46*coluna, y = 57 + 46*linha + topo do quadro + 0x3C.
// Prédios recebem os mesmos comandos como botões de TEXTO (descritor com
// ícone 0 e legenda em [+0x10], ex. "ST&OP").

enum UiCommand {
    UC_MOVE, UC_STOP, UC_BUILD_KIT, UC_BUILD_MINE, UC_WALL, UC_LAVA_WALL, UC_MICROBE_WALL, UC_TUBE,
    UC_REMOVE_WALL, UC_BULLDOZE, UC_ATTACK, UC_ORE_ROUTE, UC_STAND_GROUND, UC_GUARD, UC_PATROL,
    UC_SELF_DESTRUCT, UC_DUMP_CARGO, UC_REPAIR, UC_TAKE_OVER, UC_DISMANTLE, UC_TRANSFER, UC_SALVAGE,
    UC_SCATTER,
    // Prédios (botões de texto do quadro off_4D5220):
    UC_IDLE, UC_ACTIVATE, UC_BUILD, UC_ASSIGN_SCIENTISTS, UC_STOP_RESEARCH, UC_STOP_TRAINING,
    UC_ASSIGN_STUDENTS, UC_STORAGE, UC_DETONATE, UC_LAUNCH_PAD,
    UC_COUNT
};

// Listas na ordem do original.
const std::vector<int>& uiCommandsSingle();   // off_4E2C30 (22)
const std::vector<int>& uiCommandsMulti();    // off_4E2C88 (11)
// Quadro do prédio (off_4D5220 [+0xBC]/[+0xC0] = off_4E2D38, 12 comandos; até
// 4 viram botões de texto, sub_459F70 / layout sub_458D10).
const std::vector<int>& uiCommandsStructure();

struct UiCommandDesc {
    int iconRaw = 0;        // [+0x00] animação crua do ícone (0 = botão de texto)
    int slot = 0;           // [+0x1E] posição 1..9 na grade 3x3
    std::string tip;        // [+0x0C] dica (com '&' = tecla de atalho)
    std::string label;      // [+0x10] legenda do botão de texto
};

bool uiCommandAvailable(int cmd, const std::vector<int>& sel);   // vt[0]
UiCommandDesc uiCommandDescribe(int cmd, const std::vector<int>& sel);   // vt[2]

// Tecla de atalho da dica ('&' + letra), em maiúscula; 0 se não houver.
char uiCommandHotkey(const UiCommandDesc& d);

// vt[1]. TARGET = entra no modo de mira (o próximo clique no mapa chama
// uiCommandTarget); NOT_PORTED = comando ainda não portado (nada acontece).
enum class UiClick { DONE, TARGET, NOT_PORTED };
UiClick uiCommandClick(int cmd, const std::vector<int>& sel);
// vt[3]: alvo no tile (x,y do port). DONE = comando enviado; TARGET = segue
// no modo de mira (Patrulhar pede 2 pontos); NOT_PORTED = não portado.
UiClick uiCommandTarget(int cmd, const std::vector<int>& sel, int tileX, int tileY);
// Comandos de ÁREA (arrastar): muro/tubo (modo 4, reta) e terraplanar (modo 1).
bool uiCommandIsArea(int cmd);
bool uiCommandIsLine(int cmd);
// Fim do arrasto: retângulo em tiles com x2/y2 exclusivos. TARGET = recusado (0x58).
UiClick uiCommandArea(int cmd, const std::vector<int>& sel, int x1, int y1, int x2, int y2);
// Texto de ajuda do modo de mira (sub_411BF0 em unk_575570), ex. "Set path to
// initial patrol point"; vazio se o comando não tem.
const std::string& uiCommandHint();

// sub_43A910: solta os alvos/referências de combate da unidade (armada).
void unitResetCombat(int id);
// Ação 5 (parada) no laço de ações (sub_43ACA0 case 5).
void unitFinishStop(int id);
// Ação 0x28 (patrulhar) no laço de ações (sub_43ACA0 case 40): chegou ao último
// ponto → volta ao ponto inicial da patrulha e anda de novo.
void unitPatrolTick(int id);

} // namespace op2
