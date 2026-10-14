#include "factories_pane.h"

#include "../game/building_sheet.h"
#include "../game/colony.h"
#include "../game/structure.h"
#include "../game/tethys_game.h"
#include "../game/unit.h"

namespace op2 {

void FactoriesPane::open() {                                // sub_464AA0
    m_open = true;
    m_list.reset();
    m_unit = 0;                                             // DETAILS desabilitado (sub_464BD0)
    refresh();
}

void FactoriesPane::refresh() {                             // sub_464AF0
    m_ids.clear();
    for (int id : colonyStructures(TethysGame::localPlayer())) {
        const int t = g_units[id].type;
        if (t == 0x2F || t == 0x30 || t == 0x2E || t == 0x2D || t == 0x37) m_ids.push_back(id);
    }
    if (m_list.sel >= static_cast<int>(m_ids.size())) m_list.sel = -1;
    m_lastTick = static_cast<int>(TethysGame::tick());
}

void FactoriesPane::update() {                              // loc_464A80
    if (!m_open) return;
    const int t = static_cast<int>(TethysGame::tick());
    if (t != m_lastTick && (t & 3) == 1) refresh();
}

ListBoxLayout FactoriesPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x0A, 0, 0x12);
}

std::vector<std::string> FactoriesPane::items() const {     // sub_464C80
    std::vector<std::string> out;
    for (int id : m_ids) {
        const UnitRecord& r = g_units[id];
        std::string s = unitTitle(r);
        if (r.action == 0x12 && !(r.flags & 0x40000)) s += "\n<C5>\tIdle";
        else if (r.action == ACT_BUILD) s += "\n<C0>\tUnder Construction";
        else if (r.action == ACT_DISMANTLE) s += "\n";      // "<CO.\tDismantling": a tag sem '>' engole o texto
        else if (structureOperational(r)) {                 // vt[0xA0]
            if (r.produceType) s += std::string("\n\tProducing: ") + unitDisplayName(r.produceType);
            else s += "\n\tProducing Nothing";
        } else s += "\n<C2>\tDisabled";
        out.push_back(s);
    }
    return out;
}

void FactoriesPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   "Factories Report", 0xFF00FF00u, true);             // sub_459B60
    m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

void FactoriesPane::select(int item) {                      // sub_464C30
    if (item < 0 || item >= static_cast<int>(m_ids.size())) return;
    m_list.sel = item;
    m_unit = m_ids[item];
}

bool FactoriesPane::click(const PaneRect& pane, int x, int y, bool doubleClick, bool& dbl) {
    dbl = false;
    if (!m_open) return false;
    int item;
    if (!m_list.click(layout(pane), items(), x, y, item)) return false;
    if (item >= 0) {
        select(item);
        dbl = doubleClick;                                  // sub_464C10
    }
    return true;
}

} // namespace op2
