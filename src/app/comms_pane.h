#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"

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
//   vt[9] sub_4670A0: em jogo solo só "&JUMP" (largo), começando desabilitado.
class CommsPane {
public:
    bool isOpen() const { return m_open; }
    void open();                                         // vt[1] sub_466920
    void close() { m_open = false; }
    void update();                                       // vt[0] sub_4668C0
    void draw(uint32_t* fb, int fbW, int fbH, int winX, int winY, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y);
    bool jumpEnabled() const;                            // sub_466970
    bool jump(int& px, int& py) const;                   // sub_466A00
    int selected() const { return m_sel; }
    void select(int item) { m_sel = item; }              // testes

private:
    struct Line { int item; std::string text; };
    std::vector<Line> lines(int width) const;
    ListBoxLayout layout(const PaneRect& pane) const;

    bool m_open = false;
    int m_sel = -1;                                      // [+0xBE0] (0 = mais nova)
    int m_top = 0;                                       // 1ª linha visível
    int m_seen = 0;                                      // [+0x1C8]: nº de série da última mensagem vista
};

} // namespace op2
