#pragma once

#include <vector>

#include "weapon.h"

namespace op2 {

// Desastres — unidades "arma" do jogador 6 criadas por TethysGame::Set*.
// Todos passam pelo aviso em 3 fases (sub_4A7F70, vt[31]): 500 ticks com o
// "Warning" (se o local tem satélite EDWARD e a tecnologia de alerta), 500
// com o "ALERT" (satélite OU tecnologia), e só então agem no vt[20] (chamado
// 1x a cada 8 ticks pelo prólogo de sub_43DE50). Os que andam (sub-estado
// 0x10) movem-se a cada tick pelo vt[32].
//
//   RAIO 0x4C     off_4D0380: vt[20] sub_432F10, vt[32] sub_432E40, vt[6] sub_433680
//   TORNADO 0x4D  off_4D6458: vt[20] sub_48F430, vt[32] sub_490090, vt[6] sub_48FE10,
//                             vt[30] sub_48FBA0 (passe de sombra)
//   TERREMOTO 0x4E off_4CFB08: vt[20] sub_413790, vt[6] sub_413560 (tremor de tela)
//   ERUPÇÃO 0x4F  off_4D7428: vt[20] sub_4A81A0 → lava (dword_54E3A0, sub_432890)
//   METEORO 0x50  vt[20] sub_44A210, vt[32] sub_44ACB0, vt[6] sub_44A9A0
//
// O dano em paredes (sub_4A23F0 — damageWall, usa o RNG), o micróbio
// (dword_57AB98) e os sons estão portados.

// Coordenadas em tiles do port. endX/endY = -1 → destino sorteado.
void setEarthquake(int x, int y, int magnitude);             // magnitude 0 → 1
void setMeteor(int x, int y, int size);                      // size fora de 0..2 → sorteado
void setLightning(int x, int y, int duration, int endX, int endY);
void setTornado(int x, int y, int duration, int endX, int endY, int immediate);
void setEruption(int x, int y, int speed);
void setLavaSpeed(int speed);                                // ?SetLavaSpeed (sub_49E240)
void setLavaPossible(int x, int y, bool on);                 // ?SetLavaPossible (bit 0x10000000)

// Lava (dword_54E3A0): sub_4322A0 no início da partida; sub_49DC20 a cada turno
// (lavaTick roda antes o micróbio, dword_57AB98, como sub_489DD0).
void lavaReset();
void lavaTick();
// Micróbio: ?SetMicrobeSpreadSpeed (sub_49E240) e ?SetVirusUL (canto em tiles
// INTERNOS; spread != 0 infecta, 0 limpa).
void setMicrobeSpreadSpeed(int speed);
void setVirusUL(int x, int y, int spread);

bool isDisaster(int type);

// vt[20] do desastre. true se o tipo é um desastre tratado aqui.
bool disasterPeriodicTick(int id);
// Sub-estado 0x10 (case 16 de sub_43DE50): vt[32] do desastre que anda.
bool disasterMoveTick(int id);

// vt[6] do terremoto (sub_413560): soma o tremor de tela p/ a vista centrada em
// (viewCx, viewCy) px; o sinal alterna com (tick & frameSkip).
void disasterShake(int viewCx, int viewCy, int frameSkip, int& shakeX, int& shakeY);

// Desenho (coordenadas de mundo do port): vt[6], ou vt[30] no passe de sombra.
void disasterDrawOps(int id, bool shadowPass, std::vector<WeaponDrawOp>& ops);

} // namespace op2
