#include "ani_cursor.h"

#include <cstring>

namespace op2 {
namespace {

inline uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline bool tag(const uint8_t* p, const char* t) { return std::memcmp(p, t, 4) == 0; }

}  // namespace

// Um .CUR é um ICONDIR + ICONDIRENTRY + (BITMAPINFOHEADER, paleta, máscara XOR,
// máscara AND). A altura do cabeçalho conta as DUAS máscaras, por isso o /2.
// O ponto quente mora nos campos que no .ICO seriam "planos/bits".
bool curLoad(const uint8_t* data, size_t size, int& w, int& h, int& hotX, int& hotY,
             std::vector<uint32_t>& argb) {
    if (size < 22) return false;
    const int count = rd16(data + 4);
    if (count <= 0) return false;
    const uint8_t* e = data + 6;                       // 1ª entrada do diretório
    hotX = rd16(e + 4);
    hotY = rd16(e + 6);
    const uint32_t bytes = rd32(e + 8), off = rd32(e + 12);
    if (off + 40 > size || bytes == 0) return false;

    const uint8_t* bi = data + off;
    const int32_t bw = static_cast<int32_t>(rd32(bi + 4));
    const int32_t bh = static_cast<int32_t>(rd32(bi + 8));
    const int bpp = rd16(bi + 14);
    const uint32_t comp = rd32(bi + 16);
    if (bw <= 0 || bh <= 0 || comp != 0) return false;
    w = bw;
    h = bh / 2;                                        // XOR + AND
    if (w <= 0 || h <= 0 || w > 256 || h > 256) return false;

    uint32_t clrUsed = rd32(bi + 32);
    if (clrUsed == 0 && bpp <= 8) clrUsed = 1u << bpp;
    const uint8_t* pal = bi + rd32(bi);                // biSize (40)
    const uint8_t* xorBits = pal + clrUsed * 4;
    const int xorStride = ((w * bpp + 31) / 32) * 4;
    const int andStride = ((w + 31) / 32) * 4;
    const uint8_t* andBits = xorBits + static_cast<size_t>(xorStride) * h;
    if (andBits + static_cast<size_t>(andStride) * h > data + size) return false;

    argb.assign(static_cast<size_t>(w) * h, 0u);
    for (int y = 0; y < h; ++y) {
        const uint8_t* xrow = xorBits + static_cast<size_t>(xorStride) * (h - 1 - y);   // de baixo p/ cima
        const uint8_t* arow = andBits + static_cast<size_t>(andStride) * (h - 1 - y);
        for (int x = 0; x < w; ++x) {
            uint32_t c = 0;
            if (bpp == 32) {
                const uint8_t* p = xrow + x * 4;
                c = (static_cast<uint32_t>(p[3]) << 24) | (static_cast<uint32_t>(p[2]) << 16) |
                    (static_cast<uint32_t>(p[1]) << 8) | p[0];
            } else if (bpp == 24) {
                const uint8_t* p = xrow + x * 3;
                c = 0xFF000000u | (static_cast<uint32_t>(p[2]) << 16) |
                    (static_cast<uint32_t>(p[1]) << 8) | p[0];
            } else {
                int idx = 0;
                if (bpp == 8) idx = xrow[x];
                else if (bpp == 4) idx = (xrow[x >> 1] >> ((x & 1) ? 0 : 4)) & 0x0F;
                else if (bpp == 1) idx = (xrow[x >> 3] >> (7 - (x & 7))) & 1;
                else return false;
                const uint8_t* q = pal + static_cast<size_t>(idx) * 4;
                c = 0xFF000000u | (static_cast<uint32_t>(q[2]) << 16) |
                    (static_cast<uint32_t>(q[1]) << 8) | q[0];
            }
            // Máscara AND: bit 1 = o fundo passa. (O caso "AND=1 e XOR!=0" do
            // Windows inverte a tela; nenhum cursor do jogo usa, e aqui ele
            // vira transparente.)
            if ((arow[x >> 3] >> (7 - (x & 7))) & 1) c = 0;
            argb[static_cast<size_t>(y) * w + x] = c;
        }
    }
    (void)bytes;
    return true;
}

bool aniLoad(const uint8_t* data, size_t size, AniCursor& out) {
    if (size < 12 || !tag(data, "RIFF") || !tag(data + 8, "ACON")) return false;
    out = AniCursor{};

    size_t p = 12;
    while (p + 8 <= size) {
        const uint8_t* ck = data + p;
        const uint32_t len = rd32(ck + 4);
        const size_t body = p + 8;
        if (body + len > size) break;

        if (tag(ck, "anih") && len >= 36) {
            const uint32_t nSteps = rd32(data + body + 8);
            const uint32_t disp = rd32(data + body + 28);
            out.jiffies = disp ? static_cast<int>(disp) : 10;
            (void)nSteps;
        } else if (tag(ck, "rate")) {
            for (uint32_t i = 0; i + 4 <= len; i += 4)
                out.rates.push_back(static_cast<int>(rd32(data + body + i)));
        } else if (tag(ck, "seq ")) {
            for (uint32_t i = 0; i + 4 <= len; i += 4)
                out.seq.push_back(static_cast<int>(rd32(data + body + i)));
        } else if (tag(ck, "LIST") && len >= 4 && tag(data + body, "fram")) {
            size_t q = body + 4;
            while (q + 8 <= body + len) {
                const uint8_t* ic = data + q;
                const uint32_t ilen = rd32(ic + 4);
                if (q + 8 + ilen > size) break;
                if (tag(ic, "icon")) {
                    int w = 0, h = 0, hx = 0, hy = 0;
                    std::vector<uint32_t> px;
                    if (curLoad(data + q + 8, ilen, w, h, hx, hy, px)) {
                        if (out.frames.empty()) {
                            out.width = w; out.height = h; out.hotX = hx; out.hotY = hy;
                        }
                        if (w == out.width && h == out.height) out.frames.push_back(std::move(px));
                    }
                }
                q += 8 + ilen + (ilen & 1);            // chunks têm tamanho par
            }
        }
        p = body + len + (len & 1);
    }
    return !out.frames.empty();
}

} // namespace op2
