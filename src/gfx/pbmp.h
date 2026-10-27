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

    // Índices de paleta do tile (32 linhas; a linha seguinte fica a
    // indexStride() bytes). nullptr se o índice for inválido.
    const uint8_t* tileIndices(int index) const;
    int indexStride() const { return m_width; }

    // As 32 paletas do ciclo dia/noite (sub_4A9BF0 com N = 32, os níveis que
    // sub_4A8BD0 escolhe com `base + (escuridão << 9)`): a paleta `d` tem cada
    // canal multiplicado por min(36 − d, 32)/32 e truncado a 5 bits, ou seja o
    // dia (d ≤ 4) fica intacto e a noite plena cai a ~15%.
    static constexpr int kShadeLevels = 32;
    const uint32_t* palette(int darkness) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_bpp = 0;
    bool m_ok = false;
    std::vector<uint32_t> m_palette;   // 256 cores ARGB
    std::vector<uint32_t> m_shaded;    // kShadeLevels * 256 (sub_4A9BF0)
    std::vector<uint8_t> m_pixels;     // width*height, top-down, 8-bit
};

} // namespace op2
