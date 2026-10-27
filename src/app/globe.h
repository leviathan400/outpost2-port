#pragma once

#include <cstdint>
#include <vector>

namespace op2 {

// Visao do globo do minimapa — o objeto unk_547258 do original (o 4o botao,
// arte 0x9A, so existe nos mapas "redondos", os de largura 512 com
// dword_54EBF0 == 0: mp6_01 e newworld).
//
// O desenho nao e uma esfera texturizada de verdade: por LINHA de tela o globo
// le UMA linha da superficie do minimapa (mapeamento linear em Y) e, dentro
// dela, escolhe a coluna pela tabela de longitudes pre-calculada (sub_41FBE0),
// o que da a compressao esferica em X. O sombreamento (sub_41FEE0) vem da
// longitude absoluta: o hemisferio oposto ao sol fica preto e o resto escurece
// para as bordas.
//
//   sub_41FBE0  monta as tabelas da esfera (meia-largura, inicio e contagem de
//               cada linha, longitude de cada pixel);
//   sub_41FEE0  recalcula o sombreamento depois de girar;
//   sub_420000  desenha (spans + o retangulo da vista por cima);
//   sub_4208B0  mundo -> tela do globo;  sub_420750  tela do globo -> mundo;
//   sub_41F9E0  o giro: segue o alvo (a coluna da camera, dword_547290) com
//               aceleracao, ou e arrastado pelo mouse.
class Globe {
public:
    // sub_41FB70: a textura e a superficie do minimapa ja renderizada
    // (largura do mapa x altura + 2 margens), em RGB555.
    void setSource(const uint16_t* px, int w, int h);
    // Superficie de destino (dword_574898): o centro vai para o meio dela.
    void setTarget(int w, int h);
    void setRadius(int r);                     // sub_41F970(r, 1): 50 + zoom*10
    void setSun(int sun16);                    // sub_41F9A0 ([+0x48], GameMap::SetInitialLightLevel)
    // sub_41F910 ([+0x04]): SetDaylightEverywhere (pacote 0x2B sub-id 19)
    // desliga o sombreamento — sem noite a esfera nao tem terminador.
    // sub_41FEE0 tambem sai cedo com dword_56DEA4 ligado.
    void setShaded(bool v);
    void setSpinTarget(int rot16);             // dword_547290 (sub_408120: centrar a vista)

    int  radius() const { return m_r; }
    int  rotation() const { return m_rot; }
    void rotateBy(int d) { m_rot = (m_rot + d) & 0xFFFF; }
    void setRotation(int r) { m_rot = r & 0xFFFF; }
    void tick();                               // sub_41F9E0 (sem o mouse)
    bool ready() const { return m_src && m_rows > 0; }

    void rebuild();                            // sub_41FBE0
    void updateShade();                        // sub_41FEE0

    // sub_420000. As bordas do retangulo da vista vao em TILES do mapa real.
    void draw(uint32_t* dst, int dstStride, int ox, int oy,
              int viewLeft, int viewTop, int viewRight, int viewBottom,
              bool dragging);
    // sub_4208B0 / sub_420750 (coordenadas do mundo em 1/32 de tile).
    bool project(int wx, int wy, int& sx, int& sy) const;
    bool unproject(int sx, int sy, int& wx, int& wy) const;

private:
    int srcLineOf(int row) const;              // linha da textura de uma linha de tela
    int padLines() const;                      // margem preta da textura (altura/8)

    const uint16_t* m_src = nullptr;
    int m_srcW = 0, m_srcH = 0;
    int m_dstW = 0, m_dstH = 0;
    int m_cx = 0, m_cy = 0;                    // [+0x54] / [+0x58]
    int m_r = 60;                              // [+0x50]
    int m_rot = 0, m_sun = 0, m_phase = 0;     // [+0x4C] / [+0x48] / [+0x34]
    int m_target = 0, m_vel = 0;               // [+0x38] / [+0x3C]
    int m_skip = 0, m_top = 0, m_rows = 0;     // [+0x08] / [+0x10] / [+0x14]
    int m_lineW = 0, m_yStep = 0;              // [+0x0C] / [+0x18]
    int m_line = 0;                            // linha da textura no topo (sub_420000)
    bool m_shaded = true;                      // [+0x04] (dword_54725C, ligado por padrao)
    std::vector<int> m_halfW;                  // [+0x1C]
    std::vector<int> m_count;                  // [+0x20]
    std::vector<int> m_xstart;                 // [+0x24] (o original guarda o dobro)
    std::vector<uint8_t> m_shade;              // [+0x28]
    std::vector<uint8_t> m_col;                // [+0x2C]
    std::vector<int> m_lon;                    // [+0x30]
    uint8_t m_dither = 0x17;
};

} // namespace op2
