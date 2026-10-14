#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"
#include "report_list.h"

namespace op2 {

// Aba Factories Report — porta dword_5657C8 (vtable off_4D55C0, base de lista
// off_4D5588, provedor off_4D5508 em [+0x1C8]).
//   vt[1] sub_464AA0: lista sub_459940(.., 1, 0, 0xA, 0, 0x12) e refeita;
//   vt[0] loc_464A80: refeita quando (tick & 3) == 1 (vt[13] sub_464AF0: os
//     prédios do jogador (dword_56EEF4) dos tipos 0x2F, 0x30, 0x2E, 0x2D e 0x37,
//     na ordem da lista);
//   item (sub_464C80): título da unidade (vt[0x40]) + 2ª linha de estado:
//     "\n<C5>\tIdle" (ação 0x12 sem infecção), "\n<C0>\tUnder Construction"
//     (0x21), "\n<CO.\tDismantling" (0x22 — tag quebrada no original: o resto
//     some), "\n\tProducing: %s" / "\n\tProducing Nothing" (operacional) ou
//     "\n<C2>\tDisabled";
//   vt[4] sub_464BA0: título "Factories Report";
//   vt[9] sub_464BD0: botão largo "&DETAILS" (desabilitado até escolher);
//   vt[11] sub_464C30: escolher habilita DETAILS e guarda o prédio ([+0x2FC]);
//   vt[10] sub_464C10: clique duplo = DETAILS;
//   DETAILS (off_4E2D1C → dword_564A60 vt[1] sub_457760): prédio vivo →
//     centraliza a vista nele e o seleciona.
class FactoriesPane {
public:
    bool isOpen() const { return m_open; }
    void open();                                         // vt[1] sub_464AA0
    void close() { m_open = false; }
    void update();                                       // vt[0] loc_464A80
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    // dbl = clique duplo num item (→ DETAILS).
    bool click(const PaneRect& pane, int x, int y, bool doubleClick, bool& dbl);
    bool detailsEnabled() const { return m_open && m_unit > 0; }
    int unit() const { return m_unit; }                  // vt[14] sub_464C70
    void select(int item);                               // testes / vt[11]

private:
    void refresh();                                      // vt[13] sub_464AF0
    std::vector<std::string> items() const;              // sub_464C80
    ListBoxLayout layout(const PaneRect& pane) const;

    bool m_open = false;
    std::vector<int> m_ids;                              // [+0x1CC] (n = [+0x24C])
    ReportList m_list;
    int m_unit = 0;                                      // [+0x2FC]
    int m_lastTick = -1;
};

} // namespace op2
