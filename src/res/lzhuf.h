#pragma once

#include <cstdint>
#include <vector>

namespace op2 {

// Decoder LZHUF (LZSS + Huffman adaptativo, Okumura 1989) — compressão tipo 3
// dos blocos VBLK do formato .VOL. Portado 1:1 de tools/lzhuf.py (validado).
// Parâmetros confirmados no disassembly (sub_433F80): N=4096, F=60, THRESHOLD=2.
// As tabelas d_code/d_len foram extraídas do OUTPOST2.EXE (@0xE9700/@0xE9800),
// fonte da verdade — embutidas em lzhuf.cpp.
//
// Não há tamanho descomprimido armazenado no VOL: o decode roda até esgotar a
// entrada (a corrente é auto-delimitada pelo código de prefixo).
std::vector<uint8_t> lzhufDecompress(const uint8_t* data, size_t size);

} // namespace op2
