#pragma once

namespace op2 {

// Minimapa (o objeto unk_573818 do original) — a parte de escala/zoom:
//   sub_48E7F0  escalas de encaixe ([+8]/[+0xC], 16.16 px por TILE), zoom
//               mínimo [+0x1050] e a escala do zoom [+0x10] = [+0x14];
//   sub_48E660 / sub_48E720  Zoom In / Zoom Out (os botões 0x9D / 0x9C);
//   sub_48DB20  reduz o zoom até a vista caber na janela;
//   sub_48E090 / sub_48E390  deslocamentos [+0x105C] / [+0x1058]: a rolagem que
//               mantém o retângulo da vista dentro da janela e não deixa passar
//               das bordas do mapa (com volta nos mapas cilíndricos).
// Globais do mapa: dword_54EBC8 = largura INTERNA em tiles, dword_54EBD0 =
// altura, dword_54EBF0 = deslocamento interno em X (32 nos mapas guardados com
// a largura dobrada, 0 nos largos) — g_gameMap.internalWidth/height/XOffset.
// Mapas com deslocamento 0 são "redondos": o minimapa ganha uma margem de
// altura/8 tiles em cima e embaixo (daí o +25% no denominador da escala Y).
class MiniMap {
public:
    void configure(int winW, int winH);    // sub_48E7F0 (chamar ao carregar o mapa)
    void zoomIn();                         // sub_48E660: zoom + 1, no máximo 5
    void zoomOut();                        // sub_48E720: zoom − 1, no mínimo [+0x1050]
    int  zoom() const { return m_zoom; }
    int  minZoom() const { return m_minZoom; }
    // Vista tatica (sub_48E960): alterna [+0x104C] e refaz a superficie do
    // minimapa com as cores do TIPO de terreno (sub_437250 com a flag).
    void toggleTactical() { m_tactical = !m_tactical; }
    bool tactical() const { return m_tactical; }
    // Visao do globo (sub_48E9E0): alterna [+0x1088] e esquece o retangulo da
    // vista guardado em [+0x103C..0x1048] (no port nada e guardado: o minimapa
    // e redesenhado inteiro por quadro). So existe nos mapas "redondos", os de
    // deslocamento interno 0 — sub_458930 so cria o 4o botao com dword_54EBF0
    // zerado.
    void toggleGlobe() { m_globe = !m_globe; }
    void setTactical(bool v) { m_tactical = v; }
    void setGlobe(bool v) { m_globe = v; }
    bool globe() const { return m_globe; }

    bool canZoomIn() const { return m_zoom < 5; }
    bool canZoomOut() const { return m_zoom > m_minZoom; }

    // sub_48DB20(1) + sub_48E090(1) + sub_48E390(1): por quadro, com a vista
    // atual (camX/camY em pixels do mundo REAIS; viewW/viewH em pixels).
    void update(int camX, int camY, int viewW, int viewH);

    int scaleX() const { return m_zoom ? m_scale : m_fitX; }   // 16.16, px por tile
    int scaleY() const { return m_zoom ? m_scale : m_fitY; }
    int padY() const;                      // margem vertical em tiles (mapa redondo)
    // sub_48E5C0: nos mapas redondos sub_48DB20 desenha o minimapa DUAS
    // vezes, a segunda deslocada deste tanto, para a rolagem cilindrica
    // nao deixar buraco. 0 = nao repete.
    int wrapStep() const;
    int offX() const { return m_offX; }
    int offY() const { return m_offY; }
    // Tile → pixel na janela do minimapa (X em coordenada INTERNA).
    int pxX(int internalTileX) const { return ((internalTileX * scaleX()) >> 16) + m_offX; }
    int pxY(int tileY) const { return (((tileY + padY()) * scaleY()) >> 16) + m_offY; }

private:
    void computeScales();                  // o miolo de sub_48E7F0
    int  offsetX(int camX, int viewW, bool recompute) const;   // sub_48E090
    int  offsetY(int camY, int viewH, bool recompute) const;   // sub_48E390

    int m_winW = 0, m_winH = 0;
    int m_fitX = 0, m_fitY = 0;            // [+8] / [+0xC]
    int m_scale = 0;                       // [+0x10] = [+0x14]
    // [+0x1054] vem do "Zoom" do [Game] (sub_49ACA0, padrão 1 — e o clamp de lá
    // sempre acaba em 1); [+0x1050] é o mínimo calculado por sub_48E7F0.
    int m_zoom = 1, m_minZoom = 0;
    int m_offX = 0, m_offY = 0;            // [+0x105C] / [+0x1058]
    bool m_tactical = false;               // [+0x104C]
    bool m_globe = false;                  // [+0x1088]
};

} // namespace op2
