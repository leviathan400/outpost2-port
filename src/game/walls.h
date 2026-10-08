#pragma once

namespace op2 {

// Paredes e tubos — porta de TethysGame::CreateWallOrTube e do auto-tiling
// sub_4A1CB0. Tipos (map_id): 0x11 tubo, 0x12 parede, 0x13 parede de lava,
// 0x14 parede de micróbio → cellType 0x1A / 0x17 / 0x19 / 0x18. Paredes ganham o
// bit 31 da célula (bloqueio). Os tiles vêm dos grupos de 16 (máscara dos
// vizinhos) do tipo de terreno ("TILE SET": +0x14/+0x34/+0x54/+0x74/+0x94
// paredes, +0xBC tubo), achados por sub_4A18D0.

// CreateWallOrTube (x,y em tiles do port). Sempre devolve 1, como o original.
int createWallOrTube(int x, int y, int wallType);

// sub_4A1CB0: reajusta os vizinhos de (x,y) do mesmo tipo e o próprio tile.
void autotileWallTube(int x, int y, int wallType);

// sub_48B860: tile de tubo "isolado" (grupo +0xBC, entrada 15) p/ o tile dado.
int tubeTileFor(int tile);

// sub_4A25F0: tubo (cellType 0x1A sem o bit 31) ou parede (0x17..0x19) vira
// terraplanado (0x15) e os vizinhos perdem a ligação (sub_4A20B0). Devolve 1
// (tubo), 2 (parede) ou 0.
int destroyWallOrTube(int x, int y);

} // namespace op2
