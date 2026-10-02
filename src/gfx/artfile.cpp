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
    return m_ok;
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
            if (b1 & 0x80) o += 2;   // u16 extra
            if (b2 & 0x80) o += 2;   // u16 extra
            ArtFrame frame;
            frame.pics.reserve(npic);
            for (int pi = 0; pi < npic; ++pi) {
                if (o + 6 > n) return;
                ArtPicture pc;
                pc.image = rdU16(d, o);
                pc.flag  = d[o + 2];
                pc.slot  = d[o + 3];
                pc.x = static_cast<int16_t>(rdU16(d, o + 4));
                pc.y = static_cast<int16_t>(rdU16(d, o + 6));
                frame.pics.push_back(pc);
                o += 8;
            }
            anim.frames.push_back(std::move(frame));
        }
        if (o + 4 > n) return;
        uint32_t extra = rdU32(d, o); o += 4;
        o += static_cast<size_t>(extra) * 16;   // ExtraRecord: 4 x u32
        m_anims.push_back(std::move(anim));
    }
}

uint32_t ArtFile::palColor(int p, int c) const {
    if (p < 0 || p >= m_numPal || c < 0 || c > 255) return 0xFF000000u;
    return 0xFF000000u | (static_cast<uint32_t>(m_pal[p][c][0]) << 16)
                       | (static_cast<uint32_t>(m_pal[p][c][1]) << 8)
                       |  static_cast<uint32_t>(m_pal[p][c][2]);
}

void ArtFile::decode(int i, std::vector<uint32_t>& out, bool transparent, int palOverride) const {
    out.clear();
    if (i < 0 || i >= static_cast<int>(m_images.size())) return;
    const ArtImage& m = m_images[i];
    int p = (palOverride >= 0) ? palOverride : m.pal;
    out.assign(static_cast<size_t>(m.width) * m.height, 0u);
    for (uint32_t y = 0; y < m.height; ++y) {
        size_t src = m.offset + static_cast<size_t>(y) * m.stride;
        if (src + m.width > m_atlas.size()) break;
        for (uint32_t x = 0; x < m.width; ++x) {
            uint8_t idx = m_atlas[src + x];
            uint32_t rgba = palColor(p, idx);
            if (transparent && idx == 0) rgba = 0u;   // índice 0 = transparente
            out[static_cast<size_t>(y) * m.width + x] = rgba;
        }
    }
}

} // namespace op2
