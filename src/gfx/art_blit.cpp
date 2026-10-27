#include "art_blit.h"

#include <vector>

#include "artfile.h"

namespace op2 {

static bool s_shadows = true;   // dword_575630
void setArtShadows(bool on) { s_shadows = on; }

// Rampa do ciclo dia/noite (sub_4A9BF0 com N = 32), por canal de 8 bits.
static uint8_t s_shadeLut[32][256];
static bool s_shadeLutReady = false;

static void buildShadeLut() {
    for (int d = 0; d < 32; ++d) {
        const int mult = (32 - d + 4) < 32 ? (32 - d + 4) : 32;
        for (int v = 0; v < 256; ++v)
            s_shadeLut[d][v] = static_cast<uint8_t>(((v * mult) / 32) & 0xF8);
    }
    s_shadeLutReady = true;
}

uint32_t shadeArgb(uint32_t argb, int darkness) {
    if (darkness <= 0) return argb;
    if (darkness > 31) darkness = 31;
    if (!s_shadeLutReady) buildShadeLut();
    const uint8_t* t = s_shadeLut[darkness];
    return (argb & 0xFF000000u) | (static_cast<uint32_t>(t[(argb >> 16) & 0xFF]) << 16)
                                | (static_cast<uint32_t>(t[(argb >> 8) & 0xFF]) << 8)
                                | static_cast<uint32_t>(t[argb & 0xFF]);
}

bool blitArtFrame(uint32_t* fb, int fbW, int fbH, int animIdx, int frameIdx,
                  int cx, int cy, int playerColor,
                  int clipX0, int clipY0, int clipX1, int clipY1,
                  int darkness) {
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
        if (shadow && !s_shadows) continue;      // sub_403E00: pula a sombra
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
                    d = shadeArgb(c | 0xFF000000u, darkness);
                }
            }
        }
    }
    return true;
}

bool artFrameBounds(int animIdx, int frameIdx, int& ox, int& oy, int& w, int& h) {
    if (!g_art.ok() || animIdx < 0 || animIdx >= g_art.numAnimations()) return false;
    const ArtAnimation& an = g_art.animation(animIdx);
    if (an.frames.empty()) return false;
    const int nf = static_cast<int>(an.frames.size());
    if (frameIdx < 0 || frameIdx >= nf) frameIdx = ((frameIdx % nf) + nf) % nf;
    const int hotX = static_cast<int>(an.header[5]);
    const int hotY = static_cast<int>(an.header[6]);
    int l = 0x7FFFFFFF, t = 0x7FFFFFFF, r = -0x7FFFFFFF, b = -0x7FFFFFFF;
    for (const auto& p : an.frames[frameIdx].pics) {
        const ArtImage& m = g_art.image(p.image);
        if (m.isShadow()) continue;                 // a sombra não conta no colchete
        const int pl = p.x - hotX, pt = p.y - hotY;
        if (pl < l) l = pl;
        if (pt < t) t = pt;
        if (pl + static_cast<int>(m.width)  > r) r = pl + static_cast<int>(m.width);
        if (pt + static_cast<int>(m.height) > b) b = pt + static_cast<int>(m.height);
    }
    if (l > r) return false;
    ox = l; oy = t; w = r - l; h = b - t;
    return true;
}

} // namespace op2
