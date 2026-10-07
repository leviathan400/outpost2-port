#pragma once

#include <cstdint>
#include <vector>

#include "unit.h"

namespace op2 {

// Armas e combate — porta 1:1 do disassembly (ver [[op2-combat]] na memória).
//
// Cadeia por tick (laço sub_43AC30, na ordem da lista por Y):
//   veículo armado (flags 0x10, ou 0x80 nos prédios-torre) — cauda de
//   sub_43ACA0: off_4E1340[arma]->vt[5] = sub_4A7AF0: recarga [+0x2E], busca
//   de alvo a cada 8 ticks (vt[6]: sub_4A6DD0 com linha de visada / sub_4A6AA0
//   sem), alcance (sub_485140) e disparo (vt[4] do veículo = sub_484F80), que
//   cria a unidade-projétil do tipo da arma ([+0x24]).
//   projétil — ação 0x36 (case 54 de sub_43ACA0) com os sub-estados de
//   sub_43DE50: 0x12 gira a torre do atirador até o alvo, lança (vt[31]:
//   sub_4A3A90 balístico / sub_4A3E00 instantâneo, com mira antecipada
//   sub_4A6940), 0x13 alinha, 0x0F voa, explode (vt[34] = sub_4A35C0: dano em
//   área com queda pela distância e armadura) e espera a animação (vt[36]).
//   morte do veículo — case 47 (loc_43D8A3): explosão AMBIENT_ANIM, marca no
//   terreno, crédito de moral ao atacante ([+0x26]) e mensagem.
//
// Armas portadas: LASER 0x3D, MICROWAVE 0x3E, RAIL_GUN 0x3F, CANNON (RPG) 0x40.

// Projétil de uma das armas portadas?
bool isWeaponProjectile(int type);
// Nome de exibição da arma (desc+0x1F8) — "Laser", "RPG"...
const char* weaponDisplayName(int type);

// Cauda de sub_43ACA0: tick da arma do veículo/prédio armado (sub_4A7AF0).
void weaponTick(int id);
// vt[20] do veículo armado (sub_485340), chamado 1x a cada 8 ticks pelo
// prólogo de sub_43DE50. Só o ramo de aquisição de alvo está portado; a
// retaliação (ordem de ataque 0x24, sub_423850) depende das ordens de ataque.
void weaponAutoEngage(int id);
// Prólogo de sub_43DE50 (1x a cada 8 ticks): expira o crédito do atacante
// ([+0x26]) após 700 ticks e chama vt[20].
void unitPeriodicTick(int id);

// Case 54 de sub_43ACA0 (projétil, ação 0x36).
void weaponProjectileAction(int id);
// Sub-estados 0x0F/0x12/0x13 de sub_43DE50 (projétil). true se tratou.
bool weaponProjectileSubstate(int id);
// vt[19] (destrutor) do projétil: solta a referência ao atirador ([+0x32]).
void weaponUnitRemoved(int id);

// Case 47 de sub_43ACA0 para veículo (loc_43D8A3).
void vehicleDeath(int id);

// ---- Desenho ----------------------------------------------------------------
// Operações de desenho em coordenadas de MUNDO (pixel do original, centro).
struct WeaponDrawOp {
    enum Kind { SPRITE, LASER_LINE, MICROWAVE_LINE } kind = SPRITE;
    int anim = -1, frame = 0;       // SPRITE: animação real (sem viés) e frame
    int x = 0, y = 0;               // SPRITE: ponto; linhas: início
    int x2 = 0, y2 = 0;             // linhas: fim
    int phase = 0;                  // MICROWAVE_LINE: rotação do padrão
};
// vt[6] do projétil (sub_4A3420 laser, sub_4A3000 microwave, sub_4A2A00 rail
// gun, sub_4A2D50 genérico).
void weaponDrawOps(int id, std::vector<WeaponDrawOp>& ops);
// Torre do tanque (sub_484CF0): animação real da torre e deslocamento em
// relação ao centro do chassi (ponto de encaixe do frame do chassi). false se
// o veículo não tem torre (sem arma ou chassi sem tabela).
bool tankTurret(const UnitRecord& r, int bodyAnimReal, int bodyFrame,
                int& turretAnimReal, int& offX, int& offY);
// sub_484A90: frame do chassi do tanque (em movimento: (id + tick/2) % nF).
bool isTankChassis(int type);

} // namespace op2
