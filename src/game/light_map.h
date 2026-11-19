#pragma once

#include <cstdint>

namespace op2 {

// A luz das unidades na noite (5.41). No original, a vista guarda um mapa de
// bits com um bit por tile ([vista+0x810]); a cada quadro, sem "luz em toda
// parte" (dword_56DEA4), sub_49EA90 o zera e chama o vt[7] de cada unidade
// visivel -- sub_43A200: com a luz acesa ([+0x44] bit 0, que os veiculos
// ligam pelos farois e os predios com energia), marca um CIRCULO de raio = o
// alcance de visao da ficha (bloco do dono + 0x18, "Sight range"):
//     para dy = -r..r:  t = dy > 0 ? dy - 1 : dy;  w = isqrt(r*r - t*t)
//                       linha cy + dy, colunas cx - w .. cx + w + 1
// (byte_4E0C68 e a tabela de isqrt de 0 a 81). O tile (x, y) le os bits dos
// QUATRO cantos -- (x,y) = 1, (x+1,y) = 2, (x,y+1) = 4, (x+1,y+1) = 8, por isso
// o +1 a direita e embaixo -- e sub_4A88D0 o desenha: 0xF claro (paleta base),
// 0 com a escuridao da coluna, e o resto pixel a pixel pelo molde do VIRMASK
// (o mesmo da praga; modo 2 de sub_5865EB, 0x586746): bit ligado = claro.
// Os sprites de predios e veiculos (sub_408DB0, sub_44C910, sub_484CF0) passam
// pelo mesmo mapa (sub_470810).

// Remonta o mapa para a vista (pixels do porte) -- uma vez por quadro.
// wrapX: mapa que da a volta (largura 512).
void lightMapBuild(int camX, int camY, int viewW, int viewH, bool wrapX);
// Sem luz em toda parte e com o mapa montado?
bool lightMapActive();
// A mascara dos 4 cantos do tile (0..15). Com o mapa inativo, 15.
int lightTileMask(int tx, int ty);
// O pixel do mundo (pixels do porte) esta iluminado?
bool lightPixelLit(int wx, int wy);
// O pixel da tela, com a camera da ultima montagem (para o desenho dos sprites).
bool lightScreenPixelLit(int sx, int sy);

// O raio de luz (tiles) de uma unidade: o alcance de visao da ficha do dono,
// so com a luz acesa; 0 = nao ilumina.
int unitLightRadius(int unitId);

// Os moldes do VIRMASK.RAW (sub_49E180: big-endian e invertidos; bit 31 =
// coluna 0). Sem o arquivo, um molde de reserva por quartos.
const uint32_t* virMaskRows(int mask);
bool virMaskFromFile();

} // namespace op2
