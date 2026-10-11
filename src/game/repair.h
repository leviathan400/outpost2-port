#pragma once

#include <cstdint>
#include <vector>

namespace op2 {

// Reparar (0x1C, ConVec/Repair Vehicle/Spider), tomar (0x1D, Spider: veículo
// inimigo sob EMP) e desmontar (0x1E, ConVec: prédio próprio). Porta:
//   sub_40E210 casos 28-30 → sub_423C70: alvo em +0x48, rota até o tile do alvo
//               mais próximo (objetivo modo 3), ação = tipo.
//   jpt_43B312 casos 28-30: ao chegar ([+0x23] = 1) vira p/ o alvo
//               (sub_44C490) e espera; depois os passos sub_44CF40 (reparo),
//               sub_44D2E0 (tomada) e sub_410F80 (desmonte) a cada
//               ProductionRate/4 ticks; mensagens "Vehicle/Structure Repaired",
//               "Repairs incomplete", "Vehicle Captured/Lost".
void processRepairPacket(int owner, int type, const std::vector<int>& ids,
                         const std::vector<uint32_t>& wps, uint32_t tgt);
void repairTick(int id);           // jpt_43B312 casos 28-30 (sub-estado 0)
// sub_43DE50 case 8: espera da tomada; true = acabou (vai p/ o tick da ação).
void repairWait8(int id);
// vt[0x68] do veículo (sub_44CB60): muda o dono.
void vehicleTransfer(int id, int player);

} // namespace op2
