#pragma once

#include <cstdint>

namespace op2 {

struct MissionUnitRecord;

// Grupos de IA — porta do ScGroup (classe "Group", off_4E0B50, vt off_4CFE90)
// e do FightGroup (CombatBase, off_4E0A58, vt off_4CFEF8). São ScStubs (mesma
// tabela dos triggers); lista de instâncias dword_4E0B64 (inserção na cabeça).
//   Membros (pool de 32 nós, +0x24): {unidade, tick da última ordem, classe}
//   ordenados por classe (sub_49D1B0); a unidade guarda +0x30 = stub do grupo.
//   Por turno (sub_403110): vt[12] (sub_4281B0), DeleteWhenEmpty (sub_42B360) e
//   vt[13] (sub_428560) nos grupos habilitados.
// FightGroup: modo (sub_428470) 0 GuardRect, 1 PatrolOnly, 2 GuardGroup,
// 3 GuardUnit, 4 AttackUnit, 5 AttackEnemy, 6 ExitMap; tabela de tratadores
// off_4E0A78 (5 por modo). Veja a memória op2-ai-groups-plan.

// MiningGroup (MineGroup, off_4E0B28, vt off_4CFF58) e BuildingGroup
// (BuildGroup, off_4E0A30, vt off_4CFE58): ver src/game/ai_group.cpp.
enum GroupKind { GK_SC = 0, GK_FIGHT = 1, GK_MINE = 2, GK_BUILD = 3 };

void aiGroupsReset();
// Jogo salvo: objeto vazio p/ a tabela de stubs e as listas (dword_4E0B64,
// grupos padrão).
struct Stub;
Stub* aiGroupNewForLoad();
void aiGroupsSerializeLists(class SaveStream& s);
void aiGroupsTick();                         // sub_403110, passo dos grupos
void aiGroupsDump();                         // diagnóstico (stdout)

// Criação (CreateFightGroup / ScGroup default ctor = sem objeto).
int  aiCreateGroup(int kind, int player);    // stub (0xFF se falhou)
bool aiIsGroup(int stub);

// ScGroup
void aiTakeUnit(int stub, int unitId);       // vt[10] sub_42B960
void aiRemoveUnit(int stub, int unitId);     // vt[11] sub_42BBF0
int  aiAddUnitsFromBlock(int stub, const MissionUnitRecord* recs, int n,
                         int (*portX)(int));  // sub_42BD50
void aiSetLights(int stub, int on);          // +0x2FC
void aiSetDeleteWhenEmpty(int stub, int on); // sub_42B330
int  aiUnitCount(int stub, int cls);         // sub_42BE60 (0x11 = total)
bool aiGroupHasBeenAttacked(int stub);       // ScGroup::HasBeenAttacked (sub_42BDA0)
int  aiTotalUnitCount(int stub);             // +0x2F0
int  aiFirstOfType(int stub, int cls);       // id da 1ª unidade da classe (0 se nenhuma)
// GetFirstOfType(Unit&, tipo, carga): 1º membro do tipo na classe de (tipo,
// carga); carga 0 = qualquer arma ([+0x24]).
int  aiFirstOfTypeExact(int stub, int type, int cargo);
void aiTakeAllUnits(int stub, int fromStub);  // sub_42BD00
// Pacote 0x2A (sub_40E210 case 42): a unidade viva sai do mapa (morta, ação
// 0x2A, sub-estado 0x0B → removida no laço). Usado pelo modo 6 (ExitMap) e
// pelo TakeUnit com o grupo cheio.
void unitExitMap(int unitId);
void aiClearTargCount(int stub);              // loc_42B020
int  aiUnitClass(int type, int weapon);      // sub_49D1B0

// FightGroup (retângulos em TILES do port; o original guarda em pixels)
void aiFightSetRect(int stub, int x1, int y1, int x2, int y2);
// [+0x344] direto em px internos, depois do vt[9]; guard = também empilha em
// +0x364 (os retângulos guardados), como faz o Pinwheel.
void aiFightSetRectPx(int stub, const int rc[4], bool guard);
void aiFightAddGuardedRect(int stub, int x1, int y1, int x2, int y2);
void aiFightClearGuardedRects(int stub);
void aiFightSetMode(int stub, int mode);     // sub_428470
void aiFightSetAttackType(int stub, int type);
void aiFightSetTargetUnit(int stub, int unitId);
void aiFightSetTargetGroup(int stub, int groupStub);
void aiFightSetFollowMode(int stub, int mode);
void aiFightSetCombineFire(int stub, bool on);
// SetPatrolMode: pontos em tiles do port; start = índice de recomeço.
void aiFightSetPatrol(int stub, int n, int start, const int* xs, const int* ys);
void aiFightClearPatrol(int stub);

// TargCount (+0x2E4): SetTargCount(tipo, arma, n) (sub_42C8C0; arma 0 = qualquer)
// e SetTargCount(UnitBlock) (sub_42CBE0: +1 por veículo do bloco).
void aiSetTargCount(int stub, int type, int weapon, int count);
void aiSetTargCountBlock(int stub, const MissionUnitRecord* recs, int n);
// GroupEnumerator: id do membro no índice (0 = fim).
int  aiGroupMemberAt(int stub, int idx);
// ScStub::Destroy de um grupo: solta os membros e tira da lista.
void aiGroupDestroyed(int stub);

// MiningGroup::Setup (sub_429970): retângulo em tiles INTERNOS (MAP_RECT).
void aiMineSetup(int stub, int mineId, int smelterId, int x1, int y1, int x2, int y2);
// sub_429A50: pelas posições (tiles internos) e tipos; mina/smelter achados
// depois pela doca (ids 0).
void aiMineSetupLocs(int stub, int mx, int my, int sx, int sy, int mineType, int smelterType,
                     int x1, int y1, int x2, int y2);
// BuildingGroup (tiles INTERNOS): SetRect (sub_425AB0), RecordBuilding
// (sub_4252A0), RecordUnitBlock (sub_4254F0, a partir de `first`), RecordTube
// (sub_425090), RecordTubesTouching (sub_4257D0), RecordVehReinforceGroup (sub_4258E0).
void aiBuildSetRect(int stub, int x1, int y1, int x2, int y2);
void aiBuildRecordBuilding(int stub, int x, int y, int type, int cargo, int group);
void aiBuildRecordBlock(int stub, const MissionUnitRecord* recs, int n, int first, int group);
void aiBuildRecordTube(int stub, int x, int y);
void aiBuildRecordTubesTouching(int stub, int x, int y);
void aiBuildRecordReinforce(int stub, int group, int prio);
void aiBuildRecordWall(int stub, int x, int y, int type);   // sub_425170
void aiBuildUnRecordReinforce(int stub, int group);         // sub_4259A0

// Grupo padrão do jogador (_Player::SetDefaultGroup sub_4035A0 / GetDefaultGroup).
// Cada computador ganha do jogo um FightGroup em modo 5 (sub_402E70); unidades
// que passam para ele (vt[0x68] → sub_403080) entram nesse grupo.
void aiSetDefaultGroup(int player, int stub);
int  aiDefaultGroup(int player);
void aiDefaultGroupCreate(int player);           // sub_402E70 (GoAI)
void aiDefaultGroupsStart();                     // sub_402E10, antes do InitProc
void aiDefaultGroupDelete(int player);           // sub_402F50 (GoHuman)
void aiUnitOwnerChanged(int unitId, int player); // sub_403080

} // namespace op2
