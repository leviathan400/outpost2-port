#!/usr/bin/env python3
"""
Decoder LZHUF (LZSS + Huffman adaptativo, Haruhiko Okumura, 1989) usado pelo
Outpost 2 como compressão tipo 3 nos blocos VBLK do formato .VOL.

Confirmado por engenharia reversa do OUTPOST2.EXE:
* Construtor do decompressor (sub_433F80) preenche 0xFC4 (=4036) posições de um
  ring buffer com ' ' e inicia r=0xFC4  ->  N=4096, F=60, THRESHOLD=2.
* As tabelas de decodificação de posição d_code[256] e d_len[256] foram
  extraídas do binário (offsets de arquivo 0xE9700 e 0xE9800) — não são
  reconstruídas de memória, garantindo decode byte-exato.

Como não há um campo de "tamanho descomprimido" armazenado no .VOL, o decode
roda até esgotar os bytes de entrada (a corrente LZHUF é auto-delimitada pelo
código de prefixo; ao faltar bits para um símbolo completo, paramos).
"""

# --- parâmetros (confirmados no binário) ---
N = 4096
F = 60
THRESHOLD = 2
N_CHAR = 256 - THRESHOLD + F   # 314
T = N_CHAR * 2 - 1             # 627
R = T - 1                      # 626
MAX_FREQ = 0x8000

# Tabelas extraídas de OUTPOST2.EXE (@0xE9700 e @0xE9800)
_EXE = None


def _load_tables(exe_path="OUTPOST2.EXE"):
    """Lê d_code/d_len do binário original (fonte da verdade)."""
    with open(exe_path, "rb") as f:
        data = f.read()
    d_code = list(data[0xE9700:0xE9800])
    d_len = list(data[0xE9800:0xE9900])
    assert len(d_code) == 256 and len(d_len) == 256
    # sanity: assinaturas conhecidas
    assert d_code[-16:] == list(range(0x30, 0x40)), "d_code inesperado"
    assert d_len[:8] == [3] * 8 and d_len[-8:] == [8] * 8, "d_len inesperado"
    return d_code, d_len


class _BitReader:
    """Leitor de bits MSB-first. Levanta EOFError quando a entrada acaba."""
    def __init__(self, data):
        self.data = data
        self.pos = 0
        self.buf = 0
        self.nbits = 0

    def bit(self):
        if self.nbits == 0:
            if self.pos >= len(self.data):
                raise EOFError
            self.buf = self.data[self.pos]
            self.pos += 1
            self.nbits = 8
        self.nbits -= 1
        return (self.buf >> self.nbits) & 1

    def byte(self):
        v = 0
        for _ in range(8):
            v = (v << 1) | self.bit()
        return v


class LzhufDecoder:
    def __init__(self, exe_path="OUTPOST2.EXE"):
        self.d_code, self.d_len = _load_tables(exe_path)

    # ---- árvore de Huffman adaptativa ----
    def _start_huff(self):
        self.freq = [0] * (T + 1)
        self.prnt = [0] * (T + N_CHAR)
        self.son = [0] * T
        for i in range(N_CHAR):
            self.freq[i] = 1
            self.son[i] = i + T
            self.prnt[i + T] = i
        i, j = 0, N_CHAR
        while j <= R:
            self.freq[j] = self.freq[i] + self.freq[i + 1]
            self.son[j] = i
            self.prnt[i] = self.prnt[i + 1] = j
            i += 2
            j += 1
        self.freq[T] = 0xFFFF
        self.prnt[R] = 0

    def _reconst(self):
        freq, son, prnt = self.freq, self.son, self.prnt
        j = 0
        for i in range(T):
            if son[i] >= T:
                freq[j] = (freq[i] + 1) // 2
                son[j] = son[i]
                j += 1
        i, j = 0, N_CHAR
        while j < T:
            f = freq[j] = freq[i] + freq[i + 1]
            k = j - 1
            while f < freq[k]:
                k -= 1
            k += 1
            # desloca freq[k..j-1] -> freq[k+1..j]
            for m in range(j, k, -1):
                freq[m] = freq[m - 1]
                son[m] = son[m - 1]
            freq[k] = f
            son[k] = i
            i += 2
            j += 1
        for i in range(T):
            k = son[i]
            if k >= T:
                prnt[k] = i
            else:
                prnt[k] = prnt[k + 1] = i

    def _update(self, c):
        freq, son, prnt = self.freq, self.son, self.prnt
        if freq[R] == MAX_FREQ:
            self._reconst()
        c = prnt[c + T]
        while True:
            freq[c] += 1
            k = freq[c]
            l = c + 1
            if k > freq[l]:
                while k > freq[l + 1]:
                    l += 1
                freq[c] = freq[l]
                freq[l] = k
                i = son[c]
                prnt[i] = l
                if i < T:
                    prnt[i + 1] = l
                j = son[l]
                son[l] = i
                prnt[j] = c
                if j < T:
                    prnt[j + 1] = c
                son[c] = j
                c = l
            c = prnt[c]
            if c == 0:
                break

    def _decode_char(self, br):
        c = self.son[R]
        while c < T:
            c += br.bit()
            c = self.son[c]
        c -= T
        self._update(c)
        return c

    def _decode_position(self, br):
        i = br.byte()
        c = self.d_code[i] << 6
        j = self.d_len[i] - 2
        while j > 0:
            i = (i << 1) + br.bit()
            j -= 1
        return c | (i & 0x3F)

    def decode(self, comp_data, max_out=1 << 26):
        """Descomprime um bloco LZHUF. Roda até esgotar a entrada."""
        self._start_huff()
        text_buf = bytearray(N)
        for i in range(N - F):
            text_buf[i] = 0x20  # ' '
        r = N - F
        out = bytearray()
        br = _BitReader(comp_data)
        try:
            while len(out) < max_out:
                c = self._decode_char(br)
                if c < 256:
                    out.append(c)
                    text_buf[r] = c
                    r = (r + 1) & (N - 1)
                else:
                    i = (r - self._decode_position(br) - 1) & (N - 1)
                    j = c - 255 + THRESHOLD
                    for k in range(j):
                        ch = text_buf[(i + k) & (N - 1)]
                        out.append(ch)
                        text_buf[r] = ch
                        r = (r + 1) & (N - 1)
        except EOFError:
            pass  # entrada esgotada: fim natural do fluxo
        return bytes(out)


def decompress(comp_data, exe_path="OUTPOST2.EXE"):
    return LzhufDecoder(exe_path).decode(comp_data)


if __name__ == "__main__":
    import sys
    sys.path.insert(0, "tools")
    from vol import VolFile
    v = VolFile("SHEETS.VOL")
    dec = LzhufDecoder()
    for e in v.entries:
        raw = v.raw_block(e)
        out = dec.decode(raw)
        preview = out[:60].decode("latin-1").replace("\r", " ").replace("\n", " ")
        print(f"{e.name:14} comp={e.size:6} -> descomp={len(out):6}  {preview!r}")
