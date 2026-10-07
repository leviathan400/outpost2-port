#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Sistema de sprites do OP2 (op2_art.prt + OP2_ART.BMP). Porta o carregador
// sub_404360 ("op2_art"). .prt (metadados, no MAPS.VOL): CPAL + 9 paletas PPAL +
// numImages + ImageMeta[20 bytes] + animações. OP2_ART.BMP (raiz): atlas de pixels
// 8-bit indexados, top-down, empacotados sequencialmente. Ver [[op2-sprites-artfile]].

struct ArtImage {
    uint32_t stride = 0;   // 8bpp: scanLineByteWidth; sombra (type&4): largura em BITS
    uint32_t offset = 0;   // offset nos pixels do OP2_ART.BMP
    uint32_t height = 0;
    uint32_t width  = 0;
    uint16_t type   = 0;
    uint16_t pal    = 0;   // índice da paleta (0..8)

    // type&4 = imagem de SOMBRA: 1 BIT por pixel, não 8 (sub_403E00 seleciona o
    // modo de blit 3, e sub_4A8E10/loc_4A8F9D endereça `pix[y*stride + (x>>3)]`
    // com o bit `x&7`).
    bool isShadow() const { return (type & 4) != 0; }
    // sub_403E00: `(stride + 0x1F) & 0xFFFFFFE7` >> 3  — arredonda a largura em
    // bits para múltiplo de 32 e converte para bytes.
    uint32_t rowBytes() const {
        if (!isShadow()) return stride;
        return ((stride + 0x1Fu) & 0xFFFFFFE7u) >> 3;
    }
};

// Uma "picture" (camada) de um frame de animação.
struct ArtPicture {
    uint16_t image = 0;    // índice na tabela de imagens
    uint8_t  flag  = 0;    // flag por-picture (espelhamento?)
    uint8_t  slot  = 0;    // ordem de desenho (camada)
    int16_t  x     = 0;    // offset em pixels
    int16_t  y     = 0;
};

// Um frame = pilha de pictures.
struct ArtFrame {
    std::vector<ArtPicture> pics;
    int8_t off[4] = {0, 0, 0, 0};   // offsets x1,y1,x2,y2 (byte 0..3 do dword de sub_404DF0)
};

// Uma animação = header[9] + frames.
struct ArtAnimation {
    uint32_t header[9] = {0};
    std::vector<ArtFrame> frames;
    bool hasOffsets = false;        // base dos extras != 0xFFFF (algum frame com offsets)
};

class ArtFile {
public:
    // Carrega o .prt (via ResManager, nome no MAPS.VOL) e o BMP (caminho na raiz).
    bool load(const std::string& prtName = "op2_art.prt",
              const std::string& bmpPath = "OP2_ART.BMP");
    bool ok() const { return m_ok; }

    int numImages() const { return static_cast<int>(m_images.size()); }
    const ArtImage& image(int i) const { return m_images[i]; }

    int numAnimations() const { return static_cast<int>(m_anims.size()); }
    const ArtAnimation& animation(int i) const { return m_anims[i]; }

    // Cor RGBA (0xFFRRGGBB) de um índice de paleta numa das 9 paletas.
    uint32_t palColor(int palIndex, int colorIndex) const;

    // Decodifica a imagem `i` em RGBA (0xAARRGGBB), top-down, width*height. Usa a
    // paleta própria da imagem (ou palOverride se >= 0). O índice de cor 0 vira
    // transparente (alpha 0) quando transparent=true; senão opaco.
    //
    // playerColor >= 0 aplica o REMAP DE COR DO DONO em imagens com type&1:
    // o original (sub_4A8E10) copia 48 bytes = kPlayerRamp entradas RGB565 de
    // color.bmp (offset colorNumber*48) sobre o INÍCIO da paleta do sprite —
    // ou seja, os índices 0..23 são a rampa de cor do jogador.
    static constexpr int kPlayerRamp = 24;
    void decode(int i, std::vector<uint32_t>& out, bool transparent = true,
                int palOverride = -1, int playerColor = -1) const;

private:
    bool m_ok = false;
    std::vector<uint8_t> m_prt;
    std::vector<uint8_t> m_atlas;          // pixels do OP2_ART.BMP (a partir do offset)
    uint8_t  m_pal[9][256][3] = {{{0}}};   // r,g,b por paleta
    int      m_numPal = 0;
    std::vector<ArtImage> m_images;
    std::vector<ArtAnimation> m_anims;
    size_t   m_afterImages = 0;            // offset no .prt após as ImageMeta
    uint8_t  m_colorPal[256][3] = {{0}};   // color.bmp — rampas de cor de jogador
    bool     m_hasColorPal = false;

    void parsePrt();
    void parseAnimations(size_t startOff);
    void loadColorPalette();               // recurso "COLOR" (sub_4A8D70)
};

// Instância global (carregada uma vez), como o op2_art do engine.
extern ArtFile g_art;

} // namespace op2
