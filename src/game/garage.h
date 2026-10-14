#pragma once

namespace op2 {

// Garagem (0x29, vtable off_4CFC98) — veículos guardados em [+0x60 + 4*baia]:
//   sub_41C5F0  vt[0x104](baia): com veículo na doca e a baia vazia, guarda-o
//               (sub_44CEF0: sai do mapa e da lista, 0x20000, sub-estado 1) e
//               liga o conserto (ação 0x1B); sem veículo na doca, solta o da
//               baia (sub-estado 0x0A, célula da doca bloqueada, som 0x3D).
//   sub_41D200  vt[0xC4]: depois de 0x1E (Eden) / 0x37 ticks, quem entrava fica
//               guardado (som 0x3D) e quem saía volta ao mapa (sub_44CE60).
//   sub_41CA50  vt[0x100] (ação 0x1B, jpt_43B312 case 27): conserta até a
//               Production Capacity por passo, pagando minério (custo do veículo
//               + arma) / (2 * HP) por ponto; "Vehicle Repaired".
//   sub_41C9E0  vt[0xFC]: nº de veículos guardados.
//   sub_41D3B0  alguém entrando/saindo.
struct UnitRecord;
int  garageBays(const UnitRecord& g);              // Storage Bays (bloco+0x38)
int  garageCount(int garageId);                    // vt[0xFC]
bool garageBusy(int garageId);                     // sub_41D3B0
void garageBay(int garageId, int bay);             // vt[0x104]
void garageTick(int garageId);                     // parte da garagem em vt[0xC4]
void garageRepairStart(int garageId);              // jpt_43AD56 case 27
void garageRepairTick(int garageId);               // jpt_43B312 case 27
// Botão da baia no painel STORAGE (ramo 0x29 de sub_456BC0).
bool garageBayAvailable(int garageId, int bay);
// Veículo guardado na baia (0 = vazia).
int  garageVehicle(int garageId, int bay);

} // namespace op2
