#pragma once

namespace op2 {

// Desastres — unidades "arma" do jogador 6 criadas por TethysGame::Set*.
// Todos passam pelo aviso em 3 fases (sub_4A7F70, vt[31]): 500 ticks com o
// "Warning" (se o local tem satélite EDWARD e a tecnologia de alerta), 500
// com o "ALERT" (satélite OU tecnologia), e só então agem no vt[20] (chamado
// 1x a cada 8 ticks pelo prólogo de sub_43DE50).
//
// Portado: TERREMOTO (0x4E, vtable off_4CFB08: vt[20] = sub_413790, vt[6] =
// sub_413560 tremor de tela). Pendentes: raio 0x4C, tornado 0x4D, erupção 0x4F,
// meteoro 0x50.

// ?SetEarthquake (x,y em tiles do port; magnitude 0 → 1).
void setEarthquake(int x, int y, int magnitude);

// vt[20] do desastre. true se o tipo é um desastre tratado aqui.
bool disasterPeriodicTick(int id);

// vt[6] do terremoto (sub_413560): soma o tremor de tela p/ a vista centrada em
// (viewCx, viewCy) px; o sinal alterna com (tick & frameSkip).
void disasterShake(int viewCx, int viewCy, int frameSkip, int& shakeX, int& shakeY);

} // namespace op2
