#pragma once

namespace op2 {

// Docas dos prédios e transferência de carga com o veículo parado nelas.
//   sub_482F80  célula da doca (coluna dockLoc E/P na última linha do prédio)
//   sub_483010  veículo na doca (parado, comandável, do tipo aceito pelas
//               colunas Docking do BUILDING.TXT: All / Truck / CV)
//   sub_456BC0  botão da baia disponível (com veículo na doca: troca a carga)
//   sub_456D60  pacote 0x0A {prédio, baia} → sub_40E210 case 10 →
//               vt[0x108] (fábricas sub_415880: marca o veículo p/ a troca)
//   sub_43DE50 case 13 (sub-estado 0x0D do veículo): animação de doca em duas
//               fases; no meio, sub_411050 troca carga ↔ baia.

// Célula da doca do prédio; false se o prédio não tem doca.
bool dockCell(int buildingId, int& x, int& y);
// Veículo parado na doca (0 se nenhum).
int dockedVehicle(int buildingId);
// sub_456BC0: a baia `bay` do prédio pode ser trocada com o veículo na doca?
bool bayTransferAvailable(int buildingId, int bay);
// sub_456D60 + case 10: transfere (jogador local).
void sendBayTransfer(int buildingId, int bay);
bool processBayTransfer(int owner, int buildingId, int bay);
// Sub-estado 0x0D do veículo (1 tick).
void dockTransferTick(int vehicleId);

} // namespace op2
