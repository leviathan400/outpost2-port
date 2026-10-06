#pragma once

namespace op2 {

// Resolução de animação de veículo — porta sub_44C4F0 ([vtable+0x80], estado idle)
// + a tabela de animação do descritor (off_4E1340[type] + 0x24C, 16 direções),
// extraída do binário e mapeada por map_id via o GetName (vt[1]) de cada descritor.
//
// direction = ((rotation + 8) & 0xF0) >> 4   (16 direções; sub_44C5C0/sub_410E50)
// animIdx   = tabela[type][direction]
//
// Retorna o índice da animação no op2_art (g_art), ou -1 se o tipo não tem tabela.
int vehicleAnimIndex(int type, int rotation);

// Nº de direções válido? (tem tabela)
bool hasVehicleAnim(int type);

} // namespace op2
