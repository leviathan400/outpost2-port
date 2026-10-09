#pragma once

#include <vector>

namespace op2 {

// ConVec construindo a partir do kit — porta:
//   loc_451A20 / sub_451AC0  comando "Build structure kit" (modo de
//               posicionamento com a área do kit, sub_41B5B0)
//   sub_41B960  retângulo = área do prédio + anel de 1 tile ao redor
//   sub_451B30  pacote 6 {unidades, ponto (canto inferior direito do anel),
//               retângulo l,t,r,b}
//   sub_40E210 case 6 → vt[0x84] da ConVec (sub_423310): ação 6, ordem de
//               movimento até o ponto, retângulo em [+0x62..+0x69]
//   sub_43ACA0 jpt_43B312 case 6: ao chegar ([+0x23] = 1) vira p/ 0x60
//               (sub_44C410), soma o custo de terraplanagem e testa as células
//               (sub_4389E0 interior / sub_438B80 anel); válido → sub-estado 2;
//               senão "Building command not successful". [+0x23] = 2 → sub-estado
//               0x0A (espera a obra) e a carga some; [+0x23] = 0x0A → fim.
//   sub_43DE50 case 2: cria o prédio (sub_4369E0) em construção (ação 0x21).

// Área do kit carregado pela ConVec (false se não há kit de prédio).
bool kitFootprint(int convecId, int& w, int& h);
// Retângulo de posicionamento p/ o cursor no tile (x,y) — sub_41B960 (modo 2).
void kitPlacementRect(int w, int h, int tileX, int tileY, int& l, int& t, int& r, int& b);
// sub_451B30 + case 6 (jogador local).
void sendBuildKit(const std::vector<int>& sel, int tileX, int tileY);
// sub_452010: Robo-Miner / GeoCon — mesmo pacote 6 com a área da mina (off_4E1394).
void sendBuildMine(const std::vector<int>& sel, int tileX, int tileY);
// Área da mina (desc 0x15 +0x248/+0x24A) — o retângulo de posicionamento do
// Robo-Miner e da GeoCon (sub_451FC0).
void mineFootprint(int& w, int& h);
// sub_44C410: gira [+0x1C] rumo ao alvo (no máximo Turn Rate por tick); true = virado.
struct UnitRecord;
bool unitTurnTo(UnitRecord& u, unsigned char target);
// Tick da ação 6 (sub-estado 0, sem [+0x20]).
void conVecBuildTick(int id);
// Sub-estado 2 (sub_43DE50 case 2): cria o prédio.
void conVecBuildStart(int id);

} // namespace op2
