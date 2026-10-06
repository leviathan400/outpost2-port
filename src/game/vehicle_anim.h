#pragma once

namespace op2 {

// Resolução de animação de veículo — porta sub_44C4F0 ([vtable+0x80], estado idle)
// + a tabela de animação do descritor (off_4E1340[type] + 0x24C, 16 direções),
// extraída do binário e mapeada por map_id via o GetName (vt[1]) de cada descritor.
//
// direction = ((rotation + 8) & 0xF0) >> 4   (16 direções; sub_44C5C0/sub_410E50)
// animIdx   = tabela[type][direction]
// Tabela A (+0x24C) = Eden, Tabela B (+0x34C) = Plymouth (seleção por [unit+0x44]
// bit 0x1000 no original; aqui pela facção do dono). Veículos usam o mesmo sprite
// parado/movendo (state sempre 0 → idle), só a direção muda.
//
// Retorna o índice da animação no op2_art (g_art), ou -1 se o tipo não tem tabela.
int vehicleAnimIndex(int type, int rotation, bool isEden);

// Nº de direções válido? (tem tabela)
bool hasVehicleAnim(int type);

} // namespace op2
