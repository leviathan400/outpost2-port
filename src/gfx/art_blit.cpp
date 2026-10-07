#include "art_blit.h"

#include <vector>

#include "artfile.h"

namespace op2 {

bool blitArtFrame(uint32_t* fb, int fbW, int fbH, int animIdx, int frameIdx,
                  int cx, int cy, int playerColor,
                  int clipX0, int clipY0, int clipX1, int clipY1) {
    if (!g_art.ok() || animIdx < 0 || animIdx >= g_art.numAnimations()) return false;
    const ArtAnimation& an = g_art.animation(animIdx);
    if (an.frames.empty()) return false;
    if (frameIdx < 0 || frameIdx >= static_cast<int>(an.frames.size()))
        frameIdx = ((frameIdx % static_cast<int>(an.frames.size())) + static_cast<int>(an.frames.size()))
                   % static_cast<int>(an.frames.size());
    if (clipX0 < 0) clipX0 = 0;
    if (clipY0 < 0) clipY0 = 0;
    if (clipX1 > fbW) clipX1 = fbW;
    if (clipY1 > fbH) clipY1 = fbH;
    const int refX = cx - static_cast<int>(an.header[5]);   // hotspot X
    const int refY = cy - static_cast<int>(an.header[6]);   // hotspot Y
    // As pictures já vêm na ordem de desenho: o loader grava `pictures[slot]`
    // (loc_404B0A), então o índice do vetor É a camada (sombra, corpo, ...).
    std::vector<uint32_t> px;
    for (const auto& p : an.frames[frameIdx].pics) {
        const ArtImage& m = g_art.image(p.image);
        const bool shadow = m.isShadow();         // type&4: máscara de 1 bit por pixel
        g_art.decode(p.image, px, /*transparent*/ true, /*palOverride*/ -1, playerColor);
        for (uint32_t y = 0; y < m.height; ++y) {
            const int py = refY + p.y + static_cast<int>(y);
            if (py < clipY0 || py >= clipY1) continue;
            for (uint32_t x = 0; x < m.width; ++x) {
                const uint32_t c = px[y * m.width + x];
                if (!(c >> 24)) continue;
                const int pxs = refX + p.x + static_cast<int>(x);
                if (pxs < clipX0 || pxs >= clipX1) continue;
                uint32_t& d = fb[py * fbW + pxs];
                if (shadow) {
                    // Sombra: escurece o fundo (~50%), não desenha a cor.
                    const uint32_t rr = ((d >> 16) & 0xFF) / 2, gg = ((d >> 8) & 0xFF) / 2, bb = (d & 0xFF) / 2;
                    d = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
                } else {
                    d = c | 0xFF000000u;
                }
            }
        }
    }
    return true;
}

} // namespace op2
