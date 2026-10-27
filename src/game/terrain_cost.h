#pragma once

namespace op2 {

// Tabela de custo de movimento por terreno — porta `dword_4DEB9C` do engine
// (preenchida em sub_40B500 @asm 18551; registro 0 vem do dado estático @0x4DEB9C).
//
// Layout real: array de 32 registros de 9 dwords (36 bytes). Acesso em
// sub_4382C0/sub_438390: `dword_4DEB9C[cellType*9 + moveClass]`. Os 7 primeiros
// dwords (classes 0..6) são os valores de custo; field[7] é a cor (word) e
// field[8] é o ponteiro de nome (o nome aparece deslocado em 1 registro no asm,
// mas os custos estão corretos e alinhados ao cellType = cell & 0x1F).
//
// Semântica (confirmada em sub_4382C0/sub_438390):
//   custo == 0  → intransponível para aquela classe de movimento
//   custo  > 0  → custo-base do terreno (quanto menor, mais rápido)
// As classes 0..3 são as classes de veículo terrestre; a classe usada vem do
// descritor da unidade (dword_54F028 / [unit+0x1E8]).
//
// Número de classes de custo por registro (dwords [0..6]).
constexpr int kNumMoveClasses = 7;
constexpr int kNumCellTypes   = 32;

// Custo-base de terreno para (cellType, moveClass). Retorna 0 (intransponível)
// se os índices saírem da faixa.
int terrainMoveCost(int cellType, int moveClass);

// Cor do tipo de terreno na VISTA TATICA do minimapa (o field[8] do
// registro, +0x20, que sub_40B500 preenche): sub_437250 pinta cada tile
// com ela em vez da cor media do tile, menos as celulas com o bit 27
// (0x08000000, bloqueio duro), que ficam com a cor normal. RGB555.
unsigned short terrainTacticalColor(int cellType);

// Conveniência: o terreno é transponível para a classe se o custo != 0.
inline bool terrainPassable(int cellType, int moveClass = 0) {
    return terrainMoveCost(cellType, moveClass) != 0;
}

} // namespace op2
