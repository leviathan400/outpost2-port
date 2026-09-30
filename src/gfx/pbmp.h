#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Tileset no formato "PBMP" do Outpost 2 (well####.bmp dentro do MAPS.VOL).
// Portado 1:1 de tools/pbmp.py (validado). Uma coluna de tiles 32x32 empilhados
// (height = numTiles*32), 8-bit indexado + paleta própria.
class Pbmp {
public:
    // Faz o parse a partir dos bytes do recurso. ok() indica sucesso.
    bool load(const uint8_t* data, size_t size);
    bool ok() const { return m_ok; }

    int width()  const { return m_width; }
    int height() const { return m_height; }
    int numTiles() const { return m_height / 32; }

    // Escreve o tile 32x32 de índice `index` em `dst` (ARGB, 32 px por linha,
    // stride em px = dstStride). Retorna false se índice inválido.
    bool blitTile(int index, uint32_t* dst, int dstStride) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_bpp = 0;
    bool m_ok = false;
    std::vector<uint32_t> m_palette;   // 256 cores ARGB
    std::vector<uint8_t> m_pixels;     // width*height, top-down, 8-bit
};

} // namespace op2
