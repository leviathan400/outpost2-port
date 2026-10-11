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
int  aiTotalUnitCount(int stub);             // +0x2F0
int  aiFirstOfType(int stub, int cls);       // id da 1ª unidade da classe (0 se nenhuma)
int  aiUnitClass(int type, int weapon);      // sub_49D1B0

// FightGroup (retângulos em TILES do port; o original guarda em pixels)
void aiFightSetRect(int stub, int x1, int y1, int x2, int y2);
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
// BuildingGroup (tiles INTERNOS): SetRect (sub_425AB0), RecordBuilding
// (sub_4252A0), RecordUnitBlock (sub_4254F0, a partir de `first`), RecordTube
// (sub_425090), RecordTubesTouching (sub_4257D0), RecordVehReinforceGroup (sub_4258E0).
void aiBuildSetRect(int stub, int x1, int y1, int x2, int y2);
void aiBuildRecordBuilding(int stub, int x, int y, int type, int cargo, int group);
void aiBuildRecordBlock(int stub, const MissionUnitRecord* recs, int n, int first, int group);
void aiBuildRecordTube(int stub, int x, int y);
void aiBuildRecordTubesTouching(int stub, int x, int y);
void aiBuildRecordReinforce(int stub, int group, int prio);

// Grupo padrão do jogador (_Player::SetDefaultGroup / GetDefaultGroup).
void aiSetDefaultGroup(int player, int stub);
int  aiDefaultGroup(int player);

} // namespace op2
