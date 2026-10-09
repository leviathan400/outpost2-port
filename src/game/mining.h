#pragma once

#include <cstdint>
#include <vector>

namespace op2 {

struct UnitRecord;

// Ciclo do minério — porta:
//   sub_44AF00  MINES.TXT → tabela unk_564560 (por rendimento High/Med/Low e
//               variante 1..3: INITIAL / PEAK_TRUCK / PEAK / MIN_TRUCK / MIN,
//               em % de 1000)
//   sub_44ADF0  vt[0x100] da mina: carga = sub_44AE00 e [+0x68]++ (cargas tiradas)
//   sub_44AE00  sub_44B0B0(rendimento [+0x70], variante [+0x6C], cargas [+0x68]);
//               +20% com o "Improved" (bloco+0x1D); mina rara (0x16) → metade
//   sub_44B120  Magma Well (0x2A): carga = Production_Capacity (bloco+0x34)
//   sub_47DB40 / sub_47DC30  smelter comum/raro recebe o minério bruto: +20% com
//               o "Improved" e soma direto no estoque do jogador (corta na
//               capacidade e devolve 0)
//   sub_47DBC0 / sub_47DCB0  cabe mais (quantidade 1) no estoque?
// Caminhão (vtable off_4CF4A8): carga [+0x24] = (quantidade << 4) | tipo
//   (1 comida, 2 minério comum bruto, 3 raro bruto, 4 metal comum, 5 metal raro,
//   6..10 outros).
//   sub_406070  vt[0x88] começa a troca no prédio sob o caminhão
//   sub_406530  vt[0x94] pedido (+0x5C) e sub-estado 0x0D (carregar na mina: 1 tick
//               na 1ª fase; demais: último frame da animação de doca)
//   sub_406610  vt[0x98] troca no meio da doca
//   sub_4062B0  vt[0x80] animação (doca por tipo de carga; com carga por tabela)
//   sub_44C7B0  vt[8] frame (doca: tick − [+0x5E], travado no penúltimo)
// Rota (comando sub_453FA0 → pacote 0x27; ação 0x27 = jpt_43B312 case 39).

bool loadMinesSheet();

// Prédio do jogador `owner` cujo footprint contém o tile (x,y) — o iterador
// sub_437EB0 com a classe 4 e a máscara do dono. 0 se nenhum.
int buildingAtTile(int x, int y, int owner);

// Carga da mina (vt[0x100]); incrementa as cargas tiradas.
int mineTakeLoad(int mineId);

// Smelter: cabe `amount` (sub_47DBC0/sub_47DCB0)? / recebe (sub_47DB40/sub_47DC30).
bool smelterCanAccept(int smelterId, int amount);
bool smelterReceive(int smelterId, int amount);

// Caminhão.
void truckDockStart(int truckId);                 // vt[0x88]
void truckDockExchange(int truckId);              // vt[0x98]
int  truckAnimRaw(const UnitRecord& u, int dir);  // vt[0x80] (anim crua)
int  truckFrame(const UnitRecord& u, int id);     // vt[8]
int  truckDockPhaseTicks(const UnitRecord& u);    // [+0x3C] da 2ª fase

// Rota de minério.
// Alvo válido do clique (sub_454290): 0 = mina (0x15/0x16/0x2A), 1 = smelter
// compatível com a mina, 2 = a mesma mina; o tile tem de ser a DOCA do prédio.
int  oreRouteTarget(int stage, int mineId, int truckOwner, int tileX, int tileY);
// sub_453FA0 3º clique: pacote 0x27 (caminhões, pontos, índices e ids).
void sendOreRoute(const std::vector<int>& sel, const std::vector<uint32_t>& pts,
                  int mineIdx, int smelterIdx, int mineId, int smelterId);
void oreRouteTick(int truckId);                   // jpt_43B312 case 39

} // namespace op2
