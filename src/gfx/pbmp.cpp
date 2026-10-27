#include "pbmp.h"

#include <algorithm>

#include <cstring>

namespace op2 {

namespace {
// Varre os chunks {tag u32, len u32} a partir de `start` (até `end`) e devolve o
// offset do payload do chunk `tag` (e seu tamanho em outLen). -1 se não achar.
long findChunk(const uint8_t* d, size_t size, const char* tag, size_t start,
               size_t end, uint32_t& outLen) {
    if (end > size) end = size;
    size_t o = start;
    while (o + 8 <= end) {
        uint32_t ln;
        std::memcpy(&ln, d + o + 4, 4);
        if (std::memcmp(d + o, tag, 4) == 0) {
            outLen = ln;
            return static_cast<long>(o + 8);
        }
        o += 8 + ln;
    }
    outLen = 0;
    return -1;
}
} // namespace

bool Pbmp::load(const uint8_t* d, size_t size) {
    m_ok = false;
    if (size < 8 || std::memcmp(d, "PBMP", 4) != 0) return false;

    uint32_t hl = 0;
    long ho = findChunk(d, size, "head", 8, size, hl);
    if (ho < 0 || hl < 20) return false;
    uint32_t hdr[5];
    std::memcpy(hdr, d + ho, 20);
    // hdr[0]=ver(2) hdr[1]=width hdr[2]=height hdr[3]=bpp(8) hdr[4]=?
    m_width = static_cast<int>(hdr[1]);
    m_height = static_cast<int>(hdr[2]);
    m_bpp = static_cast<int>(hdr[3]);
    if (m_width <= 0 || m_height <= 0) return false;

    // PPAL -> data (paleta): RGBQUAD b,g,r,a
    uint32_t pl = 0;
    long po = findChunk(d, size, "PPAL", 8, size, pl);
    if (po < 0) return false;
    uint32_t pdl = 0;
    long pdo = findChunk(d, size, "data", static_cast<size_t>(po),
                         static_cast<size_t>(po) + pl, pdl);
    if (pdo < 0) return false;
    // Paleta PBMP é R,G,B,flag (convenção do OP2 — confirmado pela wiki/original:
    // idx de lava = (90,24,16) laranja como RGB). NÃO é RGBQUAD (B,G,R) do Windows.
    m_palette.assign(256, 0xFF000000u);
    uint32_t ncol = pdl / 4;
    if (ncol > 256) ncol = 256;
    for (uint32_t i = 0; i < ncol; ++i) {
        uint8_t r = d[pdo + i * 4 + 0];
        uint8_t g = d[pdo + i * 4 + 1];
        uint8_t b = d[pdo + i * 4 + 2];
        m_palette[i] = 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    }

    // pixels: o "data" de topo, logo após o "data" da paleta.
    uint32_t dl = 0;
    long pxo = findChunk(d, size, "data", static_cast<size_t>(pdo + pdl), size, dl);
    if (pxo < 0) return false;
    size_t need = static_cast<size_t>(m_width) * m_height;
    if (dl < need) return false;
    m_pixels.assign(d + pxo, d + pxo + need);

    // sub_4A9BF0(paleta, N = 32): para o nível L = N..1 (ou seja escuridão
    // d = N − L), cada canal vira canal * min(L + 4, N) / N e e truncado a 5
    // bits (a superfície do jogo e RGB555). Com d <= 4 o multiplicador satura
    // em N e a cor fica intacta.
    m_shaded.assign(static_cast<size_t>(kShadeLevels) * 256, 0xFF000000u);
    for (int d = 0; d < kShadeLevels; ++d) {
        const int mult = std::min(kShadeLevels - d + 4, kShadeLevels);
        for (int i = 0; i < 256; ++i) {
            const uint32_t c = m_palette[i];
            const int r = (static_cast<int>((c >> 16) & 0xFF) * mult) / kShadeLevels;
            const int g = (static_cast<int>((c >> 8) & 0xFF) * mult) / kShadeLevels;
            const int b = (static_cast<int>(c & 0xFF) * mult) / kShadeLevels;
            m_shaded[static_cast<size_t>(d) * 256 + i] =
                0xFF000000u | (static_cast<uint32_t>(r & 0xF8) << 16)
                            | (static_cast<uint32_t>(g & 0xF8) << 8)
                            | static_cast<uint32_t>(b & 0xF8);
        }
    }

    m_ok = true;
    return true;
}

const uint8_t* Pbmp::tileIndices(int index) const {
    if (!m_ok || index < 0 || index >= numTiles()) return nullptr;
    return &m_pixels[static_cast<size_t>(index) * 32 * m_width];
}

const uint32_t* Pbmp::palette(int darkness) const {
    if (m_shaded.empty()) return m_palette.data();
    if (darkness < 0) darkness = 0;
    if (darkness >= kShadeLevels) darkness = kShadeLevels - 1;
    return &m_shaded[static_cast<size_t>(darkness) * 256];
}

bool Pbmp::blitTile(int index, uint32_t* dst, int dstStride) const {
    if (!m_ok || index < 0 || index >= numTiles()) return false;
    size_t base = static_cast<size_t>(index) * 32 * m_width;
    for (int y = 0; y < 32; ++y) {
        const uint8_t* srcRow = &m_pixels[base + static_cast<size_t>(y) * m_width];
        uint32_t* dstRow = dst + static_cast<size_t>(y) * dstStride;
        for (int x = 0; x < 32; ++x) {
            dstRow[x] = m_palette[srcRow[x]];
        }
    }
    return true;
}

} // namespace op2
