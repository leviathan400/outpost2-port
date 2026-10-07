#pragma once

#include <cstdint>

namespace op2 {

// Blit de um frame de animação do op2_art (g_art) com o HOTSPOT da animação em
// (cx, cy) — o mesmo cálculo do blit geral do original, sub_403E00:
// destino = (cx,cy) - hotspot(header[5],header[6]) + (pic.x, pic.y), pictures
// na ordem de slot, sombra (type&4) escurecendo o fundo, cor do dono via remap.
// O clip é o retângulo [clipX0,clipX1) x [clipY0,clipY1) do framebuffer.
// Retorna false se a animação não existe.
bool blitArtFrame(uint32_t* fb, int fbW, int fbH, int animIdx, int frameIdx,
                  int cx, int cy, int playerColor,
                  int clipX0, int clipY0, int clipX1, int clipY1);

} // namespace op2
