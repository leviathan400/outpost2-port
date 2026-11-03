#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"
#include "report_list.h"

namespace op2 {

// Aba Communications — porta dword_566510 (vtable off_4D56D8, base de lista
// off_4D5588), modo 0 ([+0x1C4] = 0, "Messages"; o modo 1 "Chat" só existe em
// multijogador).
//   vt[1] sub_466920: lista sub_459940(dword_5670F4, 1, 0, 0x19, 0, 0x12);
//   vt[4] sub_466A40: título "Communications: Messages" (sub_459B60) e a linha
//     "%i: %s" (tick/100, "Current Mark") em (L+0xA, T+0x37, R−0xC, +0xF) da
//     janela do quadro (fonte unk_5772A8);
//   provedor off_4D56B8: nº = mensagens do log (dword_54F0B4), item i = a
//     i-ésima mais nova; texto (sub_466630) "%s" se x == −2 e y == −1, senão
//     "<C2>%s" (vermelho);
//   vt[0] sub_4668C0: quando (tick & 3) == 1 e chegou mensagem nova, a lista é
//     refeita e a escolha desce 1 (sub_430F30: some se passar do fim);
//   vt[11] sub_466970: escolher habilita "&JUMP" se a mensagem tem y != −1;
//   vt[10] sub_466A00 → sub_438EC0: centraliza a vista em (x, y);
//   vt[9] sub_4670A0: em jogo solo só "&JUMP" (largo), começando desabilitado;
//     em multijogador entra "&ALLIES/CHAT" ao lado dele.
//
// O MODO 1 ("Communications: Chat", sub_466A40 a partir de loc_466B67) nao e
// uma janela de conversa: e a tela "Set Up Allies and Chat Partners". Ela tem
// duas listas dos outros jogadores humanos, cada nome na cor do dono e com
// "A-" na frente de quem ME declarou aliado:
//   "Send Chat Messages To:"      caixa que liga/desliga o jogador na minha
//                                 mascara de destinatarios (local, dword_56DEEC)
//   "Players You're Allied With:" caixa que manda o pacote 0x32 (alternar a
//                                 alianca com ele)
// e embaixo a nota "A- preceding a name indicates that player is allied with
// you". As caixas sao a animacao 0x743 do op2_art (12x12; quadro 1 = marcada).
class CommsPane {
public:
    // O modo 2 e a tela "Network Statistics" (dword_567100, vtable off_4D5758),
    // que o botao "&NET STATS" da tela de aliancas abre.
    enum Mode { MESSAGES = 0, CHAT = 1, NETSTATS = 2 };

    bool isOpen() const { return m_open; }
    Mode mode() const { return m_mode; }
    void setMode(Mode m);
    void open();                                         // vt[1] sub_466920
    void close() { m_open = false; }
    void update();                                       // vt[0] sub_4668C0
    void draw(uint32_t* fb, int fbW, int fbH, int winX, int winY, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y);
    // O clique nas caixas do modo 1 (coordenadas da janela do quadro).
    bool clickChat(int winX, int winY, int x, int y);
    bool jumpEnabled() const;                            // sub_466970
    bool jump(int& px, int& py) const;                   // sub_466A00
    int selected() const { return m_list.sel; }
    void select(int item) { m_list.sel = item; }         // testes

private:
    std::vector<std::string> items() const;
    ListBoxLayout layout(const PaneRect& pane) const;
    // Os jogadores que entram nas duas listas: humanos, menos eu (sub_4670A0 e
    // sub_466A40 usam a mesma regra).
    std::vector<int> chatPlayers() const;

    bool m_open = false;
    Mode m_mode = MESSAGES;                              // [+0x1C4]
    ReportList m_list;                                   // escolha = [+0xBE0] (0 = mais nova)
    int m_seen = 0;                                      // [+0x1C8]: nº de série da última mensagem vista
};

} // namespace op2
