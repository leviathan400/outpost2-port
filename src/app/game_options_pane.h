#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "report_list.h"
#include "../res/winhelp.h"

namespace op2 {

// Painel "Game Options" (botão "&GAME OPTIONS" da aba de objetivos, sub_465830).
//
// São TRÊS telas, como no original — o porte mostrava direto a do meio, com o
// título da primeira:
//
//   MENU       os seis botões de sub_457880, com os rótulos do executável:
//              "&MISSION OBJECTIVES", "ONLINE MAN&UAL", "PR&EFERENCES",
//              "L&OAD GAME", "&SAVE GAME" e "E&ND MISSION", mais a versão no
//              rodapé (aVersion no original; aqui "Version 1.0 - beta port").
//   PREFS      as opções do [Game] do OUTPOST2.INI (a lista abaixo) mais o
//              par de radio do modo de tela (extra do porte), com
//              L&ESS/&MORE, "&APPLY" ("&Apply new values") e "&GAME OPTIONS"
//              ("Back to &Game Options"). Como no original, o que se mexe aqui
//              só vale ao APPLY: sair pelo GAME OPTIONS descarta.
//   ENDMISSION a tela de fim de missão, que no original é UMA DE DUAS
//              (sub_465830 modos 3 e 4) e com títulos próprios: em jogo solo
//              "Game End: Single Player" com "RESTART MISSIO&N", "&BACK TO
//              MAIN MENU" e "E&XIT OUTPOST 2"; em multijogador "Game End:
//              Multiplayer" com "SELF-&DESTRUCT" e "&TRANSFER" (este só
//              habilitado se algum outro humano estiver aliado a mim,
//              sub_4909F0). Cada item passa pela confirmação (casos 3 a 7 de
//              sub_457880), que executa o caso 8..0xC correspondente.
//
// A lista de PREFS tem os mesmos textos do
// original: cinco com valor — "Volume:<R>%i" (música e efeitos), "Game
// Speed:<R>%i", "Scrolling Speed:<R>%i" e "Graphic Detail Level:<R>%s"
// (Low/Medium/High) — e oito alternâncias: "Play Music", "Play Sound Effects",
// "Display Ambient Animations", "Display Computer Overlay", "Display Structure
// Shadows", "Display Vehicle Shadows", "Display Completed Objectives" e "Skip
// Frames". Os botões de baixo mudam com o item escolhido: L&ESS/&MORE nos
// valores ("Turn ... down/up") e TOGGLE nos liga/desliga ("Toggle ...").
class GameOptionsPane {
public:
    // Os 13 itens na ordem em que o original os lista, e no fim o par de
    // radio do modo de tela, que e extra deste porte.
    enum Opt {
        OPT_MUSIC_VOL, OPT_SOUND_VOL, OPT_GAME_SPEED, OPT_SCROLL_RATE, OPT_DETAIL,
        OPT_MUSIC, OPT_SOUND, OPT_AMBIENT, OPT_OVERLAY,
        OPT_STRUCT_SHADOW, OPT_VEH_SHADOW, OPT_DONE_OBJECTIVES, OPT_FRAME_SKIP,
        // Extra deste porte, que o original nao tem: o modo de tela, como um
        // PAR DE RADIO -- "(o) Full Screen" / "( ) Window". Sao DUAS linhas, e
        // nao uma alternancia so, para o grupo se comportar como os radios dos
        // dialogos do shell: o clique escolhe a linha em que se clicou, em vez
        // de inverter o que estava.
        OPT_SCREEN_FULL, OPT_SCREEN_WINDOW,
        OPT_COUNT
    };

    enum Mode { MENU, PREFS, ENDMISSION, MANUAL, MANUALTEXT };

    bool isOpen() const { return m_open; }
    Mode mode() const { return m_mode; }
    void setMode(Mode m);
    // "Open Online Man&ual": abre o OUTPOST2.HLP DENTRO da partida. O leitor é
    // o mesmo do shell (src/res/winhelp); aqui os 145 tópicos entram na lista
    // do painel, que já quebra por palavras e rola sozinha.
    void openManual();
    bool manualOk() const;
    // Captura (--panetest): abre um topico direto, sem o clique na lista.
    void debugOpenTopic(int i) { m_topic = i; m_mode = MANUALTEXT; m_list.reset(); }
    // O título da tela em que se está (off_4E8B28..off_4E8B38).
    std::string paneTitle() const;
    // Testes (--optmenutest): o texto de um item e a linha escolhida.
    std::string debugItemText(int i) const {
        const std::vector<std::string> v = items();
        return i >= 0 && i < static_cast<int>(v.size()) ? v[i] : std::string();
    }
    void debugSelect(int i) { m_list.sel = i; }
    void debugScroll(int linha) { m_list.top = linha; }   // 1a linha visivel
    void open();                                         // lê o INI
    void close() { m_open = false; }
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    // Devolve true se o clique foi consumido; `changed` indica que uma opção mudou
    // (o chamador reaplica as configurações) e `back` que o BACK foi apertado.
    bool click(const PaneRect& pane, int x, int y, bool& changed, bool& back);

    bool valueItem(int i) const { return i >= OPT_MUSIC_VOL && i <= OPT_DETAIL; }
    // As dicas dos botoes L&ESS e &MORE sao POR ITEM no original -- cada item
    // tem as suas na tabela de 7 dwords que comeca em 0x4E3118 (campos "dica
    // do botao esq./dir."). Fora dos itens com valor nao ha botao, e nao ha
    // dica.
    const char* tipLess(int i) const;
    const char* tipMore(int i) const;
    int  selected() const { return m_list.sel; }
    void step(int dir) { if (valueItem(m_list.sel)) bump(m_list.sel, dir); }   // L&ESS / &MORE
    void apply();                                        // "&APPLY": grava o INI

    // --- extra do porte: o modo de tela ---
    // O radio nao mexe na janela na hora: como todo o resto desta tela, so
    // vale no "&APPLY" -- e o APPLY grava "[Game] Fullscreen" no INI junto com
    // o resto, porque saveIni manda o espelho inteiro para o arquivo.
    void syncScreenMode(bool full);   // acerta o espelho com a janela de verdade
    bool wantFullscreen() const;      // o que o APPLY tem de aplicar
    // Testes (--optmenutest): o clique numa linha, igual ao do painel.
    void debugRowClick(int i) {
        m_list.sel = i;
        if (i >= 0 && i < OPT_COUNT && !valueItem(i)) bump(i, 0);
    }

private:
    std::vector<std::string> items() const;
    ListBoxLayout layout(const PaneRect& pane) const;
    void bump(int i, int dir);                           // L&ESS / &MORE / TOGGLE
    void saveIni() const;

    std::vector<std::string> manualItems() const;

    bool m_open = false;
    Mode m_mode = MENU;
    ReportList m_list;
    WinHelpFile m_help;
    bool m_helpTried = false;      // já tentamos ler o .HLP (não insiste a cada clique)
    int  m_topic = -1;             // tópico aberto em MANUALTEXT
};

} // namespace op2
