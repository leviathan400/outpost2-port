#pragma once

#include <cstdint>

namespace op2 {

// Pinwheel — IA de ondas de ataque dos jogos de colônia (CPS1). Porta a classe
// "PinwheelAttack" (descritor off_4E2F80, vt off_4D5410, 0x9C bytes), derivada
// de "Strategy" (off_4E4180): as estratégias ficam na lista dword_4E4194 e o
// vt[9] delas (aqui sub_45E6A0) roda em sub_403110 antes dos grupos.
//
// Cada ponto (PWDef, sub_45DFA0) tem dois retângulos — o de entrada/saída
// (rect1) e o de reunião (rect2) —, um peso (+0x20) e duas ligações (+0x24 e
// +0x28; −1 = sem) p/ girar as tropas entre pontos vizinhos. O Pinwheel cria
// três FightGroups por ponto: ataque (+0x64/+0x28), guarda (+0x74/+0x2C) e
// sapador (+0x84/+0x30), cada um com a sua composição (MrRec: tipo, arma,
// peso, mínimo; sub_45D380).
//
// Estados ([+0x48]): 0 prepara a onda (sub_45E420: força alvo pela força do
// jogador 0 × fração / composição, escolhe os pontos e os horários); 1 gera as
// unidades em rect1 nos horários (sub_45E300/sub_45D420) e elas se reúnem em
// rect2 (modo 0); 2 espera, gira tropas entre pontos ou ataca; 3 atacando
// (sub_45D7C0: ataque e sapador em AttackEnemy, guarda escoltando); 4 recuando
// p/ rect1 em ExitMap (modo 6: quem chega sai do mapa).
//
// A DLL só guarda o índice do stub (dword_56B71C).
int  pinwheelCreate(int player);                                  // CreatePinwheel
// PWDef: entradas de 11 ints {rect1 x1,y1,x2,y2, rect2 x1,y1,x2,y2 (tiles
// internos), peso, ligação0, ligação1}, até peso == −1.
void pinwheelSetPoints(int stub, const int* defs);                // SetPoints → sub_45DFA0
void pinwheelSetContactDelay(int stub, int d);                    // sub_45E260 (+0x44)
void pinwheelSetAttackFraction(int stub, int f);                  // [+0x94]
void pinwheelSetNoRange(int stub, int lo, int hi);                // sub_45E200 (+0x60/+0x5C)
void pinwheelSetWavePeriod(int stub, int lo, int hi);             // sub_45E220 (+0x58/+0x54)
// comp 0 = ataque, 1 = guarda, 2 = sapador; recs = MrRec {tipo, arma, peso,
// mínimo} até tipo 0.
void pinwheelSetComp(int stub, int comp, int lo, int hi, const int* recs);

void strategiesTick();                                            // dword_4E4194 → vt[9]
void strategiesReset();
void pinwheelDump();                                              // diagnóstico (stdout)

} // namespace op2
