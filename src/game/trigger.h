#pragma once

#include <string>
#include <vector>

namespace op2 {

// Stubs de script e triggers — porta do sistema ScStub do OUTPOST2.EXE.
//
// Tabela global de 255 stubs (dword_56B71C, índice = stubIndex; alocação
// sub_47B460 a partir do último usado, dword_56B718). Cada classe registrada
// (descritor {pai, nome, tamanho, ctor, nó, cabeça}) mantém a sua lista de
// instâncias com inserção na CABEÇA (sub_47B4F0). Classes portadas:
//   FuncReference   (off_4E3C58, sub_475710): nome + endereço da função na DLL
//   _Trigger        (off_4E9E50): [+8] habilitado, [+0x1C] oneShot,
//                   [+0x20] máscara de jogadores que dispararam, [+0x24] FuncReference
//   TimeTrigger     (off_4E9E70): [+0x30] período, [+0x34] contador (−4 a cada passada)
//   ResearchTrigger (off_4E9EB0): [+0x30] tecnologia, [+0x34] jogador
//   VictoryCondition/FailureCondition (off_4EA0B0/off_4EA0D0): [+0x34] trigger,
//                   [+0x30] texto do objetivo (memória da DLL)
// Processamento por turno: missionTriggerTick() = sub_403110.

struct MissionScript;

// ScStub (base de triggers e grupos): [+4] stubIndex, [+8] habilitado.
struct Stub {
    virtual ~Stub() = default;
    int id = 0xFF;          // +0x04
    int enabled = 0;        // +0x08
};
// sub_47B460: aloca o próximo índice livre (0xFF se a tabela encheu).
int stubAllocate(Stub* s);
// Objeto do stub (dword_56B71C[idx]) ou nullptr.
Stub* stubObject(int idx);

// Reinicia a tabela de stubs e as listas (sub_47AB00 / início de jogo).
void resetTriggers();

// Criação (exports Create*Trigger / Create*Condition). Devolvem o stubIndex
// (0xFF se falhou, como o original).
int createTimeTrigger(int bEnabled, int bOneShot, int time, const char* func);
int createResearchTrigger(int bEnabled, int bOneShot, int techID, int playerNum, const char* func);
int createVictoryCondition(int bEnabled, int bOneShot, int triggerStub, const char* text);
int createFailureCondition(int bEnabled, int bOneShot, int triggerStub, const char* text);
// CountTrigger (off_4E9FB0): jogador −1 = todos; compare_mode 0 ==, 1 <=, 2 >=, 3 <, 4 >.
int createCountTrigger(int bEnabled, int bOneShot, int player, int type, int cargo, int count,
                       int compare, const char* func);
int createKitTrigger(int bEnabled, int bOneShot, int player, int type, int count, const char* func);
int createBuildingCountTrigger(int bEnabled, int bOneShot, int player, int count, int compare, const char* func);
int createVehicleCountTrigger(int bEnabled, int bOneShot, int player, int count, int compare, const char* func);
// RectTrigger (off_4E9EF0, sub_492AC0/sub_492B10): alguma unidade do jogador
// (−1 = qualquer) no retângulo (tiles INTERNOS).
int createRectTrigger(int bEnabled, int bOneShot, int player, int x, int y, int w, int h, const char* func);
// SpecialTargetTrigger (off_4EA070, sub_495130/sub_495150): o alvo ganha a flag
// 0x2000000; dispara quando uma unidade do tipo `sourceType` termina um
// movimento ao lado dele e espera 60 ticks (ação 0x10, sub_44D5B0/sub_44D7D0).
int createSpecialTargetTrigger(int bEnabled, int bOneShot, int targetUnit, int sourceType, const char* func);
int specialTargetSource(int stub);                   // GetSpecialTargetData ([+0x34])
// sub_44D5B0: ao fim do movimento, começa a ação 0x10 se há alvo especial ao lado.
bool specialTargetCheck(int unitId);
// sub_44D7D0: fim da espera da ação 0x10.
void specialTargetArrive(int unitId);
// EscapeTrigger (off_4E9F90): retângulo em tiles INTERNOS do original.
int createEscapeTrigger(int bEnabled, int bOneShot, int player, int x, int y, int w, int h, int refValue,
                        int type, int cargoType, int cargoAmount, const char* func);
// SetTrigger (off_4E9F30): dispara p/ o jogador com >= needed dos `total` triggers.
int createSetTrigger(int bEnabled, int bOneShot, int total, int needed, const char* func, const int* stubs);
// ResourceTrigger (off_4E9E90): trig_res 0..7; jogador −1 = todos (sempre >=).
int createResourceTrigger(int bEnabled, int bOneShot, int resType, int amount, int player, int compare,
                          const char* func);
// EvacTrigger (off_4E9FF0): capacidade dos Evac Transports >= colonos.
int createEvacTrigger(int bEnabled, int bOneShot, int player, const char* func);
// OperationalTrigger (off_4EA090): prédios operando do tipo.
int createOperationalTrigger(int bEnabled, int bOneShot, int player, int type, int count, int compare,
                             const char* func);
// DamagedTrigger (off_4E9F70, sub_4934F0/sub_493510): o grupo tem menos que
// `count` unidades (sub_42BE60(0x11)).
int createDamagedTrigger(int bEnabled, int bOneShot, int groupStub, int count, const char* func);
// PointTrigger (off_4E9F10, sub_492D80/sub_492DC0): unidade do jogador (−1 =
// qualquer) sob o ponto (tiles INTERNOS; iterador sub_437840).
int createPointTrigger(int bEnabled, int bOneShot, int player, int x, int y, const char* func);
// OnePlayerLeftTrigger (off_4E9ED0, sub_492970): os humanos com Command Center
// operando são todos aliados entre si.
int createOnePlayerLeftTrigger(int bEnabled, int bOneShot, const char* func);
// Midas (sub_494900): a cada 4 ticks desconta do tempo; ao zerar, recarrega e
// dispara p/ os jogadores com mais minério comum + raro.
int createMidasTrigger(int bEnabled, int bOneShot, int time, const char* func);
// ?HasFired@Trigger: bit do jogador em [+0x20].
bool triggerHasFired(int stub, int player);

// ScStub (Enable/Disable/IsEnabled/Destroy) pelo stubIndex.
void stubEnable(int stub, bool on);
bool stubEnabled(int stub);
// ScStub::Destroy → vt[7] do objeto: sai das listas de instâncias e é apagado.
void stubDestroy(int stub);

// sub_403110: chamado a cada turno (age quando (tick & 3) == 3): dispara os
// triggers, roda o AIProc da missão e testa vitória/derrota.
void missionTriggerTick(MissionScript* mission);

// Objetivos (textos das VictoryConditions, na ordem da lista).
std::vector<std::string> missionObjectives();

// Fim de jogo (sub_48A650 / TethysGame +0x9C,+0x98): 0 = em andamento;
// 3 MISSION ACCOMPLISHED, 4 MISSION FAILED, 7 VICTORY IS SWEET, 8 YOU HAVE BEEN
// DEFEATED, 0xB demo. endTick = tick em que o jogo é encerrado.
int gameEndCode();
int gameEndTick();
const char* gameEndText(int code);     // sub_49CAC0
void setGameSpeedValue(int v);         // TethysGame +0x50 (GameStartInfo +0xAE)

} // namespace op2
