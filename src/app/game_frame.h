#pragma once

#include <cstdint>

#include "../gfx/bitmap.h"

namespace op2 {

// Moldura da janela do jogo — porta a janela-mãe do OUTPOST2.EXE (classe com
// os filhos vista/minimapa/quadro):
//   sub_499820  carrega 25 bitmaps do OUT2RES.DLL pela tabela word_4D6988
//               (handles dword_574948[i], tamanhos x/dword_5749BC[i*8]);
//   sub_499E80  área NÃO-cliente: barra de título (171 + ladrilhos 166 + 168,
//               logo "OUTPOST 2" 181 ativa / 179 inativa, botões da barra),
//               bordas laterais (182/164 e 183/165) e inferior (169/167/170);
//   sub_499BC0  fundo da área cliente: filete vertical entre a vista e a
//               coluna direita (184 + ladrilhos 178), faixa sob o minimapa
//               (186) e a barra de status (169/167/170).
//   sub_4998D0  posições dos filhos (vista, minimapa, quadro) — ver GameState.
// Coordenadas de tela 640x480: janela em (0,0), área cliente em (12,22).
class GameFrame {
public:
    bool load();                 // sub_499820 (OUT2RES.DLL na raiz)
    bool ok() const { return m_ok; }
    // sub_499E80(active): janela de wW x wH a partir de (0,0).
    void drawNonClient(uint32_t* fb, int fbW, int fbH, int wW, int wH, bool active) const;
    // sub_499BC0: área cliente em (cx, cy) com cW x cH. As janelas filhas
    // recortam a pintura da mãe: a vista (viewW x viewH no canto da área
    // cliente) não é tocada.
    void drawClient(uint32_t* fb, int fbW, int fbH, int cx, int cy, int cW, int cH, int viewW, int viewH) const;
    // Ícones dos contadores de minério da barra de status (sub_499A30): 211 comum, 210 raro.
    void drawPiece(uint32_t* fb, int fbW, int fbH, int idx, int x, int y) const;
    int width(int idx) const { return (idx >= 0 && idx < kCount) ? m_bmp[idx].getWidth() : 0; }
    int height(int idx) const { return (idx >= 0 && idx < kCount) ? m_bmp[idx].getHeight() : 0; }

    static constexpr int kCount = 25;

private:
    // BitBlt de (sx,sy,w,h) do bitmap idx p/ (dx,dy).
    void blit(uint32_t* fb, int fbW, int fbH, int idx, int dx, int dy, int sx, int sy, int w, int h) const;
    mutable int m_holeL = 0, m_holeT = 0, m_holeR = 0, m_holeB = 0;   // recorte (filho)
    Bitmap m_bmp[kCount];
    bool m_ok = false;
};

} // namespace op2
