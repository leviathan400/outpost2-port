#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "info_pane.h"
#include "produce_pane.h"
#include "report_list.h"

namespace op2 {

// Aba Space Program — porta dword_56A0A8 (vtable off_4D5AC8, base de lista
// off_4D5588, provedor off_4D5AA8 → sub_46D040). Modo em [+0x1C4]:
//   0 "Space Program: Satellites": item fixo "Communications Satellite: <C2>
//     \n\tNot Operational" e um "<nome>: \n\tOperational" por EDWARD (0x58) e
//     Solar (0x59) em órbita (sub_490800); botão "&STARSHIP" só com a
//     pesquisa do tipo 0x37 (off_4E141C);
//   1 "Space Program: Starship": módulos 0x5A..0x63 pesquisados ou lançados,
//     e 0x64..0x68 (sem tópico ou pesquisados) quando os dez módulos estão em
//     órbita (sub_4907E0); item "<C0>%s\n\tIn Orbit" / "<C2>%s\n\tResearched";
//     botão "&SATELLITES".
//   Lista sub_459940(.., 0, 0, 0xA, 0, 0x12) sem escolha; refeita quando
//   (tick & 3) == 1 (loc_46CCE0 → sub_46CD50).
class SpacePane {
public:
    bool isOpen() const { return m_open; }
    int mode() const { return m_mode; }
    void open(int mode);                                 // vt[1] sub_46CD00
    void close() { m_open = false; }
    void update();
    void draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const;
    bool click(const PaneRect& pane, int x, int y);
    static bool starshipAvailable();                     // sub_46CFA0: pesquisa do tipo 0x37

private:
    void refresh();                                      // sub_46CD50
    std::vector<std::string> items() const;              // sub_46D040
    ListBoxLayout layout(const PaneRect& pane) const { return listBoxLayout(pane, 0, 0x0A, 0, 0x12); }

    bool m_open = false;
    int m_mode = 0;
    std::vector<int> m_ids;
    ReportList m_list;
    int m_lastTick = -1;
};

} // namespace op2
