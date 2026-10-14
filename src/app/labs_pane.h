#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"
#include "report_list.h"

namespace op2 {

// Aba Labs — porta dword_568248 (vtable off_4D59B0, base de lista off_4D5588),
// com três modos em [+0x1C4] (o clique na aba volta ao 0, vt[5] sub_4648E0):
//   0 "Labs: Current Research": os labs do jogador (0x21, 0x31, 0x32 na ordem de
//     dword_56EEF4); item (sub_46BDD0) = título + "\n<C5>\tIdle" / "\n<C0>\tUnder
//     Construction" / "\n\tResearching: %s" (nome do tópico [+0x60]) /
//     "\n\tResearching Nothing" / "\n<C2>\tDisabled"; botões "&DETAILS"
//     (sub_4576D0: centraliza e seleciona o lab) e "C&OMPLETED" (→ modo 1);
//   1 "Labs: Completed Research": tópicos concluídos (sub_473270: pesquisados
//     pelo jogador, categoria != 0 e custo da facção > 0), item = nome
//     (sub_46B620); "&DETAILS" (→ modo 2 com o tópico) e "C&URRENT" (→ modo 0);
//   2 "Labs: Research Summary": a descrição do tópico (+0x2C) quebrada em 0x9B
//     px (sub_415EC0), uma linha por item, sem escolha; "C&OMPLETED"/"C&URRENT".
//   Lista: sub_459940(.., 1 (modo 2: 0), 0, 0xA, 0, 0x12); refeita quando
//   (tick & 3) == 1 (vt[0] → vt[13] sub_46B940).
class LabsPane {
public:
    bool isOpen() const { return m_open; }
    int mode() const { return m_mode; }
    void open(int mode);                                 // vt[5] + vt[1] sub_46B8A0
    void close() { m_open = false; }
    void update();
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y, bool doubleClick, bool& dbl);
    bool detailsEnabled() const;
    int lab() const { return m_lab; }                    // vt[16] sub_46BDC0 ([+0xAC0])
    int topic() const { return m_topic; }                // vt[15] sub_46BDB0 ([+0xABC])
    void select(int item);                               // vt[11] sub_46BD30

private:
    void refresh();                                      // vt[13] sub_46B940
    std::vector<std::string> items() const;
    ListBoxLayout layout(const PaneRect& pane) const;

    bool m_open = false;
    int m_mode = 0;
    std::vector<int> m_ids;                              // labs (modo 0) / tópicos (modo 1)
    std::vector<std::string> m_summary;                  // linhas do modo 2
    ReportList m_list;
    int m_lab = 0, m_topic = -1;
    bool m_chosen = false;                               // DETAILS habilitado (vt[0x14](1))
    int m_lastTick = -1;
};

} // namespace op2
