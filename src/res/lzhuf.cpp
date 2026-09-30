#include "lzhuf.h"

namespace op2 {
namespace {

// --- parâmetros (confirmados no binário, sub_433F80) ---
constexpr int N = 4096;
constexpr int F = 60;
constexpr int THRESHOLD = 2;
constexpr int N_CHAR = 256 - THRESHOLD + F;   // 314
constexpr int T = N_CHAR * 2 - 1;             // 627
constexpr int R = T - 1;                      // 626
constexpr int MAX_FREQ = 0x8000;

// Tabelas extraídas de OUTPOST2.EXE (@0xE9700 e @0xE9800).
static const uint8_t kDCode[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15,
    16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19,
    20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23,
    24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31,
    32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39,
    40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};
static const uint8_t kDLen[256] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

// Leitor de bits MSB-first. hitEof=true quando a entrada acaba (fim natural).
struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;
    int buf = 0;
    int nbits = 0;
    bool eof = false;

    BitReader(const uint8_t* d, size_t s) : data(d), size(s) {}

    int bit() {
        if (nbits == 0) {
            if (pos >= size) { eof = true; return 0; }
            buf = data[pos++];
            nbits = 8;
        }
        --nbits;
        return (buf >> nbits) & 1;
    }
    int byte() {
        int v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 1) | bit();
        return v;
    }
};

// Árvore de Huffman adaptativa (estado por descompressão).
struct Huff {
    uint16_t freq[T + 1];
    int prnt[T + N_CHAR];
    int son[T];

    void start() {
        for (int i = 0; i < N_CHAR; ++i) {
            freq[i] = 1;
            son[i] = i + T;
            prnt[i + T] = i;
        }
        int i = 0, j = N_CHAR;
        while (j <= R) {
            freq[j] = static_cast<uint16_t>(freq[i] + freq[i + 1]);
            son[j] = i;
            prnt[i] = prnt[i + 1] = j;
            i += 2;
            ++j;
        }
        freq[T] = 0xFFFF;
        prnt[R] = 0;
    }

    void reconst() {
        int j = 0;
        for (int i = 0; i < T; ++i) {
            if (son[i] >= T) {
                freq[j] = static_cast<uint16_t>((freq[i] + 1) / 2);
                son[j] = son[i];
                ++j;
            }
        }
        int i = 0;
        for (j = N_CHAR; j < T; ++j) {
            uint16_t f = freq[j] = static_cast<uint16_t>(freq[i] + freq[i + 1]);
            int k = j - 1;
            while (f < freq[k]) --k;
            ++k;
            for (int m = j; m > k; --m) {
                freq[m] = freq[m - 1];
                son[m] = son[m - 1];
            }
            freq[k] = f;
            son[k] = i;
            i += 2;
        }
        for (i = 0; i < T; ++i) {
            int k = son[i];
            if (k >= T) prnt[k] = i;
            else prnt[k] = prnt[k + 1] = i;
        }
    }

    void update(int c) {
        if (freq[R] == MAX_FREQ) reconst();
        c = prnt[c + T];
        do {
            int k = ++freq[c];
            int l = c + 1;
            if (k > freq[l]) {
                while (k > freq[l + 1]) ++l;
                freq[c] = freq[l];
                freq[l] = static_cast<uint16_t>(k);
                int i = son[c];
                prnt[i] = l;
                if (i < T) prnt[i + 1] = l;
                int jj = son[l];
                son[l] = i;
                prnt[jj] = c;
                if (jj < T) prnt[jj + 1] = c;
                son[c] = jj;
                c = l;
            }
            c = prnt[c];
        } while (c != 0);
    }

    int decodeChar(BitReader& br) {
        int c = son[R];
        while (c < T) {
            c += br.bit();
            c = son[c];
        }
        c -= T;
        update(c);
        return c;
    }

    int decodePosition(BitReader& br) {
        int i = br.byte();
        int c = kDCode[i] << 6;
        int j = kDLen[i] - 2;
        while (j-- > 0) i = (i << 1) + br.bit();
        return c | (i & 0x3F);
    }
};

} // namespace

std::vector<uint8_t> lzhufDecompress(const uint8_t* data, size_t size) {
    Huff h;
    h.start();
    uint8_t textBuf[N];
    for (int i = 0; i < N - F; ++i) textBuf[i] = 0x20;  // ' '
    int r = N - F;
    std::vector<uint8_t> out;
    BitReader br(data, size);

    while (true) {
        int c = h.decodeChar(br);
        if (br.eof) break;
        if (c < 256) {
            out.push_back(static_cast<uint8_t>(c));
            textBuf[r] = static_cast<uint8_t>(c);
            r = (r + 1) & (N - 1);
        } else {
            int pos = h.decodePosition(br);
            if (br.eof) break;
            int i = (r - pos - 1) & (N - 1);
            int j = c - 255 + THRESHOLD;
            for (int k = 0; k < j; ++k) {
                uint8_t ch = textBuf[(i + k) & (N - 1)];
                out.push_back(ch);
                textBuf[r] = ch;
                r = (r + 1) & (N - 1);
            }
        }
    }
    return out;
}

} // namespace op2
