#pragma once

namespace op2 {

// Destroço (WRECK_FLAG 0x54, vtable off_4CF400), entulho e beacons — o que os
// veículos fazem ao CHEGAR num tile (fim de sub_43ACA0 casos 2-4) e o caminho
// do destroço até a pesquisa:
//   sub_405BF0  vt[2] do destroço: some em 0x0B, sobre terraplanado ou sob um
//               prédio; Scouts a < 160 px o descobrem (vt[0x98] sub_405B10).
//   sub_405A50  vt[6]: anim 0x7A5 (cor do jogador 1) se descoberto por um aliado
//               do jogador local.
//   sub_406C20  caminhão vazio parado em entulho (0x16) ou num destroço que o
//               dono conhece → ação 0x0F (espera 30; jpt_43B312 case 15 carrega:
//               entulho 6/0xD7 ou 7/0xFA e o tile vira terraplanado; destroço
//               9/tecnologia − 8000, "Wreckage Loaded").
//   sub_47F470  vt[0x108](baia, 0) do Spaceport: troca imediata baia ↔ caminhão;
//               carga 9 vira a tecnologia pesquisada (sub_472CD0 + sub_473830).
//   sub_406070 / sub_406610 caso 57: GORF recebe entulho (sub_46E4E0 / sub_46E550).
//   sub_484460  Robo-Surveyor parado num beacon → ação 0x11 (espera 30; case 17:
//               vt[0x9C] sub_405420 pesquisa e sub_44BE40 sai de cima).

// Gancho de chegada (sem 0x40 e quando sub_44D5B0 não tratou).
void unitArrived(int id);
// Ações 0x0F / 0x11: início (ramo [+0x20]) e tick.
void salvageActionStart(int id);
bool salvageActionTick(int id);      // true se tratou
// vt[2] do destroço; true = remover.
bool wreckTick(int id);
// Desenho (sub_405A50): anim crua e frame, ou false.
bool wreckSprite(int id, int& animRaw, int& frame);
// sub_47F470 com flag 0. false se recusado.
bool spaceportBayTransfer(int owner, int spaceportId, int bay);
// GORF: entulho entregue (sub_46E4E0 comum / sub_46E550 raro).
void gorfReceiveRubble(int gorfId, int kind, int amount);
// vt[39] do beacon (sub_405420).
void beaconSurveyBy(int beaconId, int player);
// Destroço: vt[0x98] sub_405B10 (descobre p/ o jogador, mensagem p/ o local) e
// vt[0x9C] sub_405B80 (algum descobridor aliado do jogador).
struct UnitRecord;
void wreckDiscover(int id, int player);
bool wreckKnownBy(const UnitRecord& w, int player);

} // namespace op2
