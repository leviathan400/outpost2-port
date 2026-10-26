#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "report_list.h"

namespace op2 {

// Painel "Game Options" (botão "&GAME OPTIONS" da aba de objetivos, sub_465830).
// A lista tem as opções do [Game] do OUTPOST2.INI, com os mesmos textos do
// original: cinco com valor — "Volume:<R>%i" (música e efeitos), "Game
// Speed:<R>%i", "Scrolling Speed:<R>%i" e "Graphic Detail Level:<R>%s"
// (Low/Medium/High) — e oito alternâncias: "Play Music", "Play Sound Effects",
// "Display Ambient Animations", "Display Computer Overlay", "Display Structure
// Shadows", "Display Vehicle Shadows", "Display Completed Objectives" e "Skip
// Frames". Os botões de baixo mudam com o item escolhido: L&ESS/&MORE nos
// valores ("Turn ... down/up") e TOGGLE nos liga/desliga ("Toggle ...").
class GameOptionsPane {
public:
    // Os 13 itens, na ordem em que o original os lista.
    enum Opt {
        OPT_MUSIC_VOL, OPT_SOUND_VOL, OPT_GAME_SPEED, OPT_SCROLL_RATE, OPT_DETAIL,
        OPT_MUSIC, OPT_SOUND, OPT_AMBIENT, OPT_OVERLAY,
        OPT_STRUCT_SHADOW, OPT_VEH_SHADOW, OPT_DONE_OBJECTIVES, OPT_FRAME_SKIP,
        OPT_COUNT
    };

    bool isOpen() const { return m_open; }
    void open();                                         // lê o INI
    void close() { m_open = false; }
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    // Devolve true se o clique foi consumido; `changed` indica que uma opção mudou
    // (o chamador reaplica as configurações) e `back` que o BACK foi apertado.
    bool click(const PaneRect& pane, int x, int y, bool& changed, bool& back);

    bool valueItem(int i) const { return i >= OPT_MUSIC_VOL && i <= OPT_DETAIL; }
    int  selected() const { return m_list.sel; }
    void step(int dir) { if (valueItem(m_list.sel)) bump(m_list.sel, dir); }   // L&ESS / &MORE

private:
    std::vector<std::string> items() const;
    ListBoxLayout layout(const PaneRect& pane) const;
    void bump(int i, int dir);                           // L&ESS / &MORE / TOGGLE
    void saveIni() const;

    bool m_open = false;
    ReportList m_list;
};

} // namespace op2
