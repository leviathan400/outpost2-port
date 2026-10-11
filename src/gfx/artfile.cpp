#include "artfile.h"

#include <cstring>
#include <fstream>
#include <iostream>

#include "../res/res_manager.h"

namespace op2 {

ArtFile g_art;

static uint32_t rdU32(const uint8_t* d, size_t o) {
    uint32_t v; std::memcpy(&v, d + o, 4); return v;
}
static uint16_t rdU16(const uint8_t* d, size_t o) {
    uint16_t v; std::memcpy(&v, d + o, 2); return v;
}

bool ArtFile::load(const std::string& prtName, const std::string& bmpPath) {
    m_ok = false;
    // op2_art.prt (no MAPS.VOL, via ResManager)
    auto stream = g_ResManager.openStream(prtName);
    if (!stream) { std::cerr << "[Art] " << prtName << " nao encontrado" << std::endl; return false; }
    m_prt.assign(stream->data(), stream->data() + stream->size());

    // OP2_ART.BMP (raiz) — atlas de pixels; começa no offset do header BMP (@10).
    std::ifstream f(bmpPath, std::ios::binary);
    if (!f) { std::cerr << "[Art] " << bmpPath << " nao encontrado" << std::endl; return false; }
    std::vector<uint8_t> bmp((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    if (bmp.size() < 14) return false;
    uint32_t pixOff = rdU32(bmp.data(), 10);
    if (pixOff >= bmp.size()) return false;
    m_atlas.assign(bmp.begin() + pixOff, bmp.end());

    parsePrt();
    loadColorPalette();
    return m_ok;
}

// sub_4A8D70: carrega o recurso "COLOR" (color.bmp, no MAPS.VOL) e monta a tabela
// de remap. O arquivo é um BMP 1x1 de 8 bits — o que importa é só a PALETA de 256
// cores, em grupos de 24 (uma rampa por colorNumber de jogador).
void ArtFile::loadColorPalette() {
    auto stream = g_ResManager.openStream("color.bmp");
    if (!stream || stream->size() < 54 + 1024) {
        std::cerr << "[Art] color.bmp nao encontrado — sem remap de cor" << std::endl;
        return;
    }
    const uint8_t* d = stream->data();
    const size_t hdr = 14 + rdU32(d, 14);        // 14 (file hdr) + BITMAPINFOHEADER
    if (hdr + 1024 > stream->size()) return;
    for (int i = 0; i < 256; ++i) {               // quads BGRA
        m_colorPal[i][0] = d[hdr + i * 4 + 2];    // r
        m_colorPal[i][1] = d[hdr + i * 4 + 1];    // g
        m_colorPal[i][2] = d[hdr + i * 4 + 0];    // b
    }
    m_hasColorPal = true;
}

void ArtFile::parsePrt() {
    const uint8_t* d = m_prt.data();
    size_t n = m_prt.size();
    if (n < 8 || std::memcmp(d, "CPAL", 4) != 0) {
        std::cerr << "[Art] .prt invalido (sem CPAL)" << std::endl; return;
    }
    m_numPal = static_cast<int>(rdU32(d, 4));
    if (m_numPal > 9) m_numPal = 9;

    // Paletas: cada PPAL tem um sub-chunk 'data' com 1024 bytes (RGB+flag x256).
    size_t o = 8;
    for (int p = 0; p < m_numPal; ++p) {
        if (o + 4 > n || std::memcmp(d + o, "PPAL", 4) != 0) {
            std::cerr << "[Art] PPAL faltando em " << p << std::endl; return;
        }
        // acha "data" a partir de o
        size_t do_ = 0;
        for (size_t s = o; s + 4 <= n; ++s)
            if (std::memcmp(d + s, "data", 4) == 0) { do_ = s + 8; break; }
        if (do_ == 0 || do_ + 1024 > n) return;
        for (int i = 0; i < 256; ++i) {
            m_pal[p][i][0] = d[do_ + i * 4 + 0];   // r
            m_pal[p][i][1] = d[do_ + i * 4 + 1];   // g
            m_pal[p][i][2] = d[do_ + i * 4 + 2];   // b
        }
        o = do_ + 1024;
    }

    // numImages + ImageMeta[20 bytes]
    if (o + 4 > n) return;
    uint32_t numImg = rdU32(d, o); o += 4;
    m_images.reserve(numImg);
    for (uint32_t i = 0; i < numImg; ++i) {
        if (o + 20 > n) return;
        ArtImage m;
        m.stride = rdU32(d, o);
        m.offset = rdU32(d, o + 4);
        m.height = rdU32(d, o + 8);
        m.width  = rdU32(d, o + 12);
        m.type   = rdU16(d, o + 16);
        m.pal    = rdU16(d, o + 18);
        m_images.push_back(m);
        o += 20;
    }
    m_afterImages = o;

    parseAnimations(o);
    m_ok = true;
}

void ArtFile::parseAnimations(size_t startOff) {
    const uint8_t* d = m_prt.data();
    size_t n = m_prt.size();
    size_t o = startOff;
    if (o + 16 > n) return;
    uint32_t numAnim = rdU32(d, o);
    o += 16;   // pula numAnim, numFrames, numPictures, numFrameExtra
    m_anims.reserve(numAnim);
    for (uint32_t a = 0; a < numAnim; ++a) {
        if (o + 36 > n) return;
        ArtAnimation anim;
        for (int k = 0; k < 9; ++k) anim.header[k] = rdU32(d, o + k * 4);
        o += 36;
        uint32_t nf = anim.header[8];
        anim.frames.reserve(nf);
        for (uint32_t fr = 0; fr < nf; ++fr) {
            if (o + 2 > n) return;
            uint8_t b1 = d[o]; uint8_t b2 = d[o + 1]; o += 2;
            int npic = b1 & 0x7F;
            ArtFrame frame;
            // Offsets opcionais do frame (loc_404A45..loc_404AF6): com b1&0x80 o
            // loader lê 2 bytes p/ os bytes 0,1 do dword extras[base+frame]; com
            // b2&0x80, 2 bytes p/ os bytes 2,3. É o que sub_404DF0 devolve (ponto
            // de encaixe da torre / boca da arma, em int8 x,y).
            if (o + (b1 & 0x80 ? 2 : 0) + (b2 & 0x80 ? 2 : 0) > n) return;
            if (b1 & 0x80) { frame.off[0] = static_cast<int8_t>(d[o]); frame.off[1] = static_cast<int8_t>(d[o + 1]); o += 2; }
            if (b2 & 0x80) { frame.off[2] = static_cast<int8_t>(d[o]); frame.off[3] = static_cast<int8_t>(d[o + 1]); o += 2; }
            if (b1 & 0x80 || b2 & 0x80) anim.hasOffsets = true;
            // O byte +3 de cada registro é o ÍNDICE de destino, não um campo
            // qualquer: o loader original grava `pictures[slot]` (loc_404B0A,
            // `eax = slot; [edx + slot*6 + 4] = imageIndex`). A ordem de leitura
            // != ordem de desenho, e o blit percorre o array em ordem de slot —
            // é o que põe a sombra embaixo e o overlay de cor em cima.
            frame.pics.assign(npic, ArtPicture{});
            for (int pi = 0; pi < npic; ++pi) {
                if (o + 8 > n) return;
                ArtPicture pc;
                pc.image = rdU16(d, o);
                pc.flag  = d[o + 2];
                pc.slot  = d[o + 3];
                pc.x = static_cast<int16_t>(rdU16(d, o + 4));
                pc.y = static_cast<int16_t>(rdU16(d, o + 6));
                if (pc.slot < npic) frame.pics[pc.slot] = pc;
                o += 8;
            }
            anim.frames.push_back(std::move(frame));
        }
        if (o + 4 > n) return;
        uint32_t extra = rdU32(d, o); o += 4;
        if (o + static_cast<size_t>(extra) * 16 > n) return;
        for (uint32_t k = 0; k < extra; ++k, o += 16)        // ExtraRecord: 4 x u32
            anim.clips.push_back({static_cast<int32_t>(rdU32(d, o)), static_cast<int32_t>(rdU32(d, o + 4)),
                                  static_cast<int32_t>(rdU32(d, o + 8)), static_cast<int32_t>(rdU32(d, o + 12))});
        m_anims.push_back(std::move(anim));
    }
}

bool ArtFile::pixelMiss(int anim, int frame, int x, int y, int clipAnim) const {
    if (anim < 0 || anim >= numAnimations()) return true;
    const ArtAnimation& a = m_anims[anim];
    if (frame < 0 || frame >= static_cast<int>(a.frames.size())) return true;
    const ArtAnimation& c = (clipAnim >= 0 && clipAnim < numAnimations()) ? m_anims[clipAnim] : a;
    x += static_cast<int32_t>(c.header[1]);
    y += static_cast<int32_t>(c.header[2]);
    const int hx = static_cast<int32_t>(a.header[5]), hy = static_cast<int32_t>(a.header[6]);
    for (const ArtPicture& p : a.frames[frame].pics) {            // ordem de slot
        if (p.image >= m_images.size()) continue;
        const ArtImage& m = m_images[p.image];
        const int px = x - p.x, py = y - p.y;
        const int k = (m.type & 0x3E0) >> 5;
        if (k) {                                                   // recorte k (relativo ao hotspot)
            const std::array<int32_t, 4> r = k - 1 < static_cast<int>(a.clips.size())
                                                 ? a.clips[k - 1] : std::array<int32_t, 4>{0, 0, 0, 0};
            if (r[0] - hx > px || px > r[2] - hx || py < r[1] - hy || py > r[3] - hy) continue;
        }
        // (largura/altura inclusivas e o stride cru — em bits nas sombras —
        // como no original.)
        if (px < 0 || py < 0 || static_cast<int>(m.width) < px || static_cast<int>(m.height) < py) continue;
        const size_t off = static_cast<size_t>(m.offset) + static_cast<size_t>(m.stride) * py + px;
        if (off < m_atlas.size() && m_atlas[off] != 0) return false;
    }
    return true;
}

uint32_t ArtFile::palColor(int p, int c) const {
    if (p < 0 || p >= m_numPal || c < 0 || c > 255) return 0xFF000000u;
    return 0xFF000000u | (static_cast<uint32_t>(m_pal[p][c][0]) << 16)
                       | (static_cast<uint32_t>(m_pal[p][c][1]) << 8)
                       |  static_cast<uint32_t>(m_pal[p][c][2]);
}

void ArtFile::decode(int i, std::vector<uint32_t>& out, bool transparent, int palOverride,
                     int playerColor) const {
    out.clear();
    if (i < 0 || i >= static_cast<int>(m_images.size())) return;
    const ArtImage& m = m_images[i];
    out.assign(static_cast<size_t>(m.width) * m.height, 0u);

    // Sombra: 1 bit por pixel. O blit do original (modo 3) lê o bit
    // `pix[y*rowBytes + (x>>3)] & (x&7)` e escurece o destino; aqui marcamos o
    // pixel como preto opaco e quem desenha aplica o escurecimento.
    if (m.isShadow()) {
        const uint32_t rb = m.rowBytes();
        for (uint32_t y = 0; y < m.height; ++y) {
            size_t src = m.offset + static_cast<size_t>(y) * rb;
            if (src + rb > m_atlas.size()) break;
            for (uint32_t x = 0; x < m.width; ++x) {
                uint8_t b = m_atlas[src + (x >> 3)];
                if (b & (0x80u >> (x & 7)))
                    out[static_cast<size_t>(y) * m.width + x] = 0xFF000000u;
            }
        }
        return;
    }

    int p = (palOverride >= 0) ? palOverride : m.pal;

    // Remap de cor do dono: só vale para imagens marcadas com type&1 (sub_403E00
    // passa a cor do jogador apenas nesse caso; senão passa 0xFF).
    const bool remap = m_hasColorPal && playerColor >= 0 && (m.type & 1) != 0;
    uint32_t ramp[kPlayerRamp];
    if (remap) {
        const int base = playerColor * kPlayerRamp;
        for (int k = 0; k < kPlayerRamp; ++k) {
            int c = (base + k) & 0xFF;
            ramp[k] = 0xFF000000u | (static_cast<uint32_t>(m_colorPal[c][0]) << 16)
                                  | (static_cast<uint32_t>(m_colorPal[c][1]) << 8)
                                  |  static_cast<uint32_t>(m_colorPal[c][2]);
        }
    }

    for (uint32_t y = 0; y < m.height; ++y) {
        size_t src = m.offset + static_cast<size_t>(y) * m.stride;
        if (src + m.width > m_atlas.size()) break;
        for (uint32_t x = 0; x < m.width; ++x) {
            uint8_t idx = m_atlas[src + x];
            uint32_t rgba = (remap && idx < kPlayerRamp) ? ramp[idx] : palColor(p, idx);
            if (transparent && idx == 0) rgba = 0u;   // índice 0 = transparente
            out[static_cast<size_t>(y) * m.width + x] = rgba;
        }
    }
}

} // namespace op2
