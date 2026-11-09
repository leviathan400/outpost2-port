#pragma once

#include <cstdint>

namespace op2 {

// Blit de um frame de animação do op2_art (g_art) com o HOTSPOT da animação em
// (cx, cy) — o mesmo cálculo do blit geral do original, sub_403E00:
// destino = (cx,cy) - hotspot(header[5],header[6]) + (pic.x, pic.y), pictures
// na ordem de slot, sombra (type&4) escurecendo o fundo, cor do dono via remap.
// O clip é o retângulo [clipX0,clipX1) x [clipY0,clipY1) do framebuffer.
// Retorna false se a animação não existe.
// dword_575630: sombras (pictures type&4) desenhadas? O desenho de cada classe
// copia a opção do INI antes de blitar — StationaryShadows (dword_575634) p/
// prédios, MobileShadows (dword_575638) p/ veículos e efeitos (sub_405160).
void setArtShadows(bool on);

// `darkness` 0..31 é o nível do ciclo dia/noite (sub_470810 recebe o índice já
// calculado da coluna do sprite e escolhe a paleta escurecida). 0 = dia pleno.
bool blitArtFrame(uint32_t* fb, int fbW, int fbH, int animIdx, int frameIdx,
                  int cx, int cy, int playerColor,
                  int clipX0, int clipY0, int clipX1, int clipY1,
                  int darkness = 0);

// A mesma rampa das paletas de sub_4A9BF0 aplicada a um ARGB: cada canal vira
// canal * min(36 − d, 32) / 32, truncado a 5 bits.
uint32_t shadeArgb(uint32_t argb, int darkness);

// Retângulo que o frame ocupa RELATIVO ao ponto (cx,cy) passado ao blit (i.e.,
// já descontado o hotspot). Exclui as pictures de sombra (type&4). Equivale ao
// que vt[0x30]/vt[0x34] do original devolvem para o colchete de seleção
// (sub_43E950). Devolve false se o frame não tem pixels. (ox,oy) = canto
// superior-esquerdo; (w,h) = dimensões.
bool artFrameBounds(int animIdx, int frameIdx, int& ox, int& oy, int& w, int& h);

} // namespace op2
