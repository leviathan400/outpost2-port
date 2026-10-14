#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"
#include "report_list.h"

namespace op2 {

// Aba Resources — porta dword_567920 (vtable off_4D5888, base de lista
// off_4D5588, provedor off_4D5868 → sub_46A600). Modo em [+0x1C4]
// (dword_567AE4; o clique na aba volta ao 0):
//   0  "Resources Report": lista 1..5 (+6 com a pesquisa da mina de raro) —
//      Colonists / Morale Level / Power Use / Food Stored / Common Metals /
//      Rare Metals; "&DETAILS" abre o modo do item escolhido;
//   1..6 páginas de status (sub_4699D0 Colonists, sub_469B90 Morale + lista
//      dos fatores, sub_469E40 Power, sub_469FD0 Food, sub_46A280 / sub_46A440
//      Common / Rare Metals); botões "S&TRUCTURES" (→ modo + 6) e "&SUMMARY";
//   7..12 listas de prédios do jogador por grupo (sub_468B60), item = título
//      + estado (+ ", <rendimento> (%i)" nas minas); "&DETAILS" (centraliza e
//      seleciona, off_4E2D20) e "&SUMMARY".
//   Refeita quando (tick & 3) == 1 (sub_468A20 → vt[13] sub_468B60).
// Alt+C/M/P/F/N/R (comandos 40200..40205 de sub_498890) abrem os modos 1..6
// sem mexer na seleção; Alt+R só com a pesquisa da mina de raro.
class ResourcesPane {
public:
    bool isOpen() const { return m_open; }
    int mode() const { return m_mode; }
    void open(int mode);                                 // vt[1] sub_468AC0
    void close() { m_open = false; }
    void update();                                       // vt[0] sub_468A20
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y, bool doubleClick, bool& dbl);
    bool detailsEnabled() const;
    int detailsMode() const { return m_detailMode; }     // [+0x3A8]
    int unit() const { return m_unit; }                  // vt[14] sub_4696A0 ([+0x45C])
    void select(int item);                               // vt[11] sub_469630
    static bool rareAvailable();                         // pesquisa do tipo 0x16 (off_4E1398)

private:
    void refresh();                                      // vt[13] sub_468B60
    std::vector<std::string> items() const;              // sub_46A600
    ListBoxLayout layout(const PaneRect& pane) const;
    bool hasList() const { return m_mode == 0 || m_mode == 2 || m_mode >= 7; }

    bool m_open = false;
    int m_mode = 0;
    std::vector<int> m_ids;                              // [+0x1CC] (n = [+0x24C])
    ReportList m_list;
    int m_detailMode = 0, m_unit = 0;
    bool m_chosen = false;
    int m_lastTick = -1;
};

} // namespace op2
