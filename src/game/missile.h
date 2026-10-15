#pragma once

#include <vector>

#include "weapon.h"

namespace op2 {

// Lançamentos do Spaceport (classe off_4D4448: SULV 0x69, RLV 0x6A, míssil EMP
// 0x6B) e a explosão EMP_BIG (0x4A, classe off_4D3BE8). Ação 0x1A:
//   jpt_43AD56 case 26 — decolagem: sub-estado 0x0A pela animação; som 0x41;
//                        voz 0x7D/0x7E do dono, ou "Missile Launch Detected".
//   jpt_43B312 case 26 — pelo sub-estado anterior [+0x23]:
//     0x0A: 1ª vez sobe 0x9B px e espera 0x2D (flag 0x4000); 2ª vez vt[0x7C]
//           (em órbita) e espera 6 (RLV 0x4B0, míssil 100; 0xC com "rápidas").
//     6:    vt[0x80] (sub_480280) vai ao alvo [+0x34]/[+0x38]; desce 0x2D (sub-estado 9).
//     9:    com 0x4000, 2ª fase da descida (animação); senão vt[0x84]
//           (sub_480590): cria a EMP_BIG no alvo e morre (ação 0x2F).
//   vt[0x50] (sub_4802A0, 1x a cada 8 ticks): na descida, Meteor Defenses de
//   outros jogadores a <= 20 tiles derrubam (50%/75%) e avisa "Incoming Missile".

bool isLaunchUnit(int type);     // 0x69..0x6B
bool isEmpBlast(int type);       // 0x4A

// ?SetEMPMissile: míssil em (x, y) do dono, alvo (tx, ty) — tiles do port.
int setEMPMissile(int x, int y, int owner, int tx, int ty);
int missileCampaignRLV(int owner, int delay);   // sub_4898F0 (RLV em órbita)
// sub_40E210 case 25 (pacote 0x19) → sub_424140: lança a carga da plataforma do
// Spaceport ([+0x6E], com o módulo [+0x72]) do ponto de lançamento do sprite;
// alvo em px do port. false se recusado.
bool spaceportLaunch(int spaceportId, int tx, int ty);
// LAUNCH (sub_4572F0): um Spaceport operando com carga na plataforma; SULV
// exige o módulo; o de evacuação (0x67), 200 colonos.
bool spaceportLaunchAvailable(int spaceportId);
// Baia → plataforma (dword_564A3C, loc_456DD0): plataforma com veículo que não
// é o míssil, e baia ≠ carga atual (alguma das duas não vazia).
bool spaceportPadSwapAvailable(int spaceportId, int bay);
// Pacote 0x0A com a palavra +0x12 = 1 → vt[0x108](baia, 1) (sub_47F470): troca a
// baia com a carga do SULV ([+0x72]).
bool spaceportPadSwap(int owner, int spaceportId, int bay);
// _Player::CaptureRLV(de) / GetRLVCount (contador +0x08 bits 0..3).
void playerCaptureRLV(int player, int from);

void missileActionStart(int id);   // jpt_43AD56 case 26
void missileActionTick(int id);    // jpt_43B312 case 26
// vt[0x50] do lançamento (sub_4802A0) / da EMP_BIG (sub_413DF0). false se o
// tipo não é desta família.
bool missilePeriodicTick(int id);
// vt[6]: sub_4805F0 (lançamento) / sub_413BC0 (anel da EMP_BIG).
void missileDrawOps(int id, std::vector<WeaponDrawOp>& ops);

// vt[0x5C] (EMP por `ticks`/8 passadas): veículo sub_44B5F0 (apaga as luzes,
// para a ação, sub-estado 0x0A se não anda), prédio sub_4836E0 (economia refeita).
void unitApplyEMP(int id, int ticks);

} // namespace op2
