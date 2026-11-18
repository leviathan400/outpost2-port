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

// sub_44C4F0 por inteiro: o descritor tem QUATRO conjuntos de 16 direcoes --
// 0 parado, 1 andando, 2 trabalhando (acao = desc+0x44C), 3 acao = desc+0x450.
// vehicleAnimSet escolhe o conjunto como o original: sub-estado 1 com
// velocidade ou giro ([st+0x58]/[st+0x5C]/[st+0x60]) = 1; fora do sub-estado 1,
// a acao igual a desc+0x44C = 2, a desc+0x450 = 3; senao 0. Entrada vazia no
// conjunto cai na do conjunto 0.
int vehicleAnimSet(int type, int action, int subState, bool movingNow);
int vehicleAnimIndexSet(int type, int rotation, bool isEden, int set);
int vehicleWorkAction(int type, int which);   // desc+0x44C (0) / +0x450 (1); 0x35 = nenhuma

// Nº de direções válido? (tem tabela)
bool hasVehicleAnim(int type);

// Code_Name do descritor (ex. "CON_TRUCK") p/ map_id 0x01..0x10, ou nullptr.
const char* vehicleCodeName(int type);

} // namespace op2
