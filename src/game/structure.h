#pragma once

#include <string>

#include "unit.h"

namespace op2 {

// Ciclo de vida das ESTRUTURAS e das unidades-EFEITO (AMBIENT_ANIM), portado
// 1:1 do disassembly. Ver [[op2-sprites-artfile]] (seção ESTRUTURAS).
//
// Laço por tick (sub_43AC30): para cada unidade, vt[1] (máquina de ações,
// sub_43ACA0) e depois vt[2] (atualização: sub_408910 → sub_408930 +
// sub_43DE50). Se vt[2] retornar ≠0 a unidade é desligada e destruída.
//
// Estado do prédio = [+0x34] = (estado<<16)|contador:
//   0 intacto/construção — frame = progresso; contador 0 = último frame
//   1 dano >= 30% do HP máx, 2 dano >= 60%   (sub_408930, frame 0)
//   3 destruição — frame = (nF-1) - contador, contador desce 1 por tick
//   5..9 animações ambiente (sub_4835C0 — ainda não portado)

// Kill (sub_43A890): [+0x21]=0x2F, [+0x20]=1, [+0x3C]=delay. É o que
// Unit::DoDeath chama com delay 1.
void killUnit(int id, int delay);

// Início da construção de um prédio por um ConVec — cauda de sub_43DE50
// (case 2, @103780). Duração = BuildPoints * ProductionRate(ConVec)/128 (+ o
// [+0x3C] do ConVec), /2 em certos tipos de missão (sub_4027D0), &~3.
void structureStartBuild(int id, int builderId);

// HP máximo do prédio p/ o dono (desc[8 + jogador*0x44], BUILDING.TXT).
int structureMaxHP(const UnitRecord& r);

// Animação (índice real no op2_art, -1 se nenhuma) e frame atuais do prédio:
// sub_4835A0 (anim do estado) + sub_409220 (frame).
int structureCurAnim(const UnitRecord& r);
int structureFrame(const UnitRecord& r);

// Cria uma unidade-efeito AMBIENT_ANIM (sub_4466B0(0x72) + sub_405000) que toca
// `animRaw` (com viés 0x65) e morre ao fim. pixelX/Y = canto do tile (port).
int createAmbientAnim(int pixelX, int pixelY, int animRaw, int div, int delay);

// Animação/frame de uma unidade-efeito AMBIENT_ANIM (sub_405160). Retorna false
// se não deve ser desenhada (antes do atraso, ou escondida).
bool effectAnimFrame(const UnitRecord& r, int& anim, int& frame);

// Laço de unidades não-veículo (prédios e efeitos) — sub_43AC30. Os veículos
// seguem em tickMovement() (vt[1]/vt[2] de veículo ainda não portados).
void tickUnits();

// Remove a unidade do pool (sub_439CE0 + destrutor): libera a ocupação e o slot.
void removeUnit(int id);

// Animações ambiente (estados 5..9) — sub_4835C0. Exigem o prédio OPERANDO
// (sub_483750: energia + trabalhadores + cientistas, dano < 75%), estado que
// vem da economia da colônia (src/game/colony.cpp, sub_471F30).
bool structureIsOperational(const UnitRecord& r);
// sub_483810: liga trabalhadores, cientistas e energia de uma vez.
void structureTurnOn(UnitRecord& r);
// Opção "Ambient" do OUTPOST2.INI (dword_575628).
void setAmbientAnimations(bool on);

// Nome exibido da unidade (vt+0x40 = sub_43A5E0): "%s # %i-%02i" com o nome
// do descritor (+0x1F8), jogador+1 e o nº de série (+0x2C). Guard Post /
// Lynx / Panther / Tiger levam o nome da arma antes (entra com as armas).
std::string unitTitle(const UnitRecord& r);

} // namespace op2
