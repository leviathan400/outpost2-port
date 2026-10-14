#include "labs_pane.h"

#include "../gfx/ui_font.h"
#include "../game/colony.h"
#include "../game/player.h"
#include "../game/research.h"
#include "../game/structure.h"
#include "../game/tethys_game.h"
#include "../game/unit.h"

namespace op2 {

void LabsPane::open(int mode) {                             // sub_46B8A0
    m_open = true;
    m_mode = mode;
    m_list.reset();
    m_chosen = false;                                       // sub_46BB90: DETAILS começa desabilitado
    refresh();
}

// sub_415EC0: quebra por palavras na largura dada (texto sem tags).
static std::vector<std::string> wrapPlain(const std::string& text, int width) {
    std::vector<std::string> out;
    std::string cur, word;
    for (size_t k = 0; k <= text.size(); ++k) {
        if (k == text.size() || text[k] == ' ' || text[k] == '\n') {
            const std::string cand = cur.empty() ? word : cur + " " + word;
            if (!cur.empty() && UiFont::width(cand) > width) { out.push_back(cur); cur = word; }
            else cur = cand;
            word.clear();
            if (k < text.size() && text[k] == '\n') { out.push_back(cur); cur.clear(); }
        } else {
            word += text[k];
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void LabsPane::refresh() {                                  // sub_46B940
    const int me = TethysGame::localPlayer();
    m_ids.clear();
    if (m_mode == 0) {
        for (int id : colonyStructures(me)) {
            const int t = g_units[id].type;
            if (t == 0x21 || t == 0x31 || t == 0x32) m_ids.push_back(id);
        }
    } else if (m_mode == 1) {                               // sub_473270
        const bool eden = g_playerData[me].isEden != 0;
        for (int i = 1; i < techCount(); ++i) {
            const Tech* t = techAt(i);
            if (!t || !researchHas(me, i) || t->category == 0) continue;
            if ((eden ? t->edenCost : t->plymouthCost) <= 0) continue;
            m_ids.push_back(i);
        }
    } else {
        const Tech* t = techAt(m_topic);
        m_summary = wrapPlain(t ? t->description.substr(0, 0x28A) : std::string(), 0x9B);
    }
    if (m_mode != 2 && m_list.sel >= static_cast<int>(m_ids.size())) m_list.sel = -1;
    m_lastTick = static_cast<int>(TethysGame::tick());
}

void LabsPane::update() {                                   // loc_46B880
    if (!m_open) return;
    const int t = static_cast<int>(TethysGame::tick());
    if (t != m_lastTick && (t & 3) == 1) refresh();
}

ListBoxLayout LabsPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x0A, 0, 0x12);
}

std::vector<std::string> LabsPane::items() const {
    std::vector<std::string> out;
    if (m_mode == 2) return m_summary;
    for (int id : m_ids) {
        if (m_mode == 1) {                                  // sub_46B620
            const Tech* t = techAt(id);
            out.push_back(t ? t->name : std::string());
            continue;
        }
        const UnitRecord& r = g_units[id];                  // sub_46BDD0
        std::string s = unitTitle(r);
        if (r.action == 0x12 && !(r.flags & 0x40000)) s += "\n<C5>\tIdle";
        else if (r.action == ACT_BUILD) s += "\n<C0>\tUnder Construction";
        else if (structureOperational(r)) {                 // vt[0xA0]
            const Tech* t = r.labTopic ? techAt(r.labTopic) : nullptr;
            if (r.labTopic && t) s += "\n\tResearching: " + t->name;
            else s += "\n\tResearching Nothing";
        } else s += "\n<C2>\tDisabled";
        out.push_back(s);
    }
    return out;
}

void LabsPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    static const char* kTitle[3] = {"Labs: Current Research", "Labs: Completed Research", "Labs: Research Summary"};
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   kTitle[m_mode], 0xFF00FF00u, true);                 // sub_46BB10 → sub_459B60
    m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

void LabsPane::select(int item) {                           // sub_46BD30
    if (m_mode == 2 || item < 0 || item >= static_cast<int>(m_ids.size())) return;
    m_list.sel = item;
    m_chosen = true;
    if (m_mode == 0) m_lab = m_ids[item];                   // [+0xAC0]
    else m_topic = m_ids[item];                             // [+0xABC]
}

bool LabsPane::click(const PaneRect& pane, int x, int y, bool doubleClick, bool& dbl) {
    dbl = false;
    if (!m_open) return false;
    const int keep = m_list.sel;
    int item;
    if (!m_list.click(layout(pane), items(), x, y, item)) return false;
    if (m_mode == 2) { m_list.sel = keep; return true; }    // lista sem escolha (sub_459940 com 0)
    if (item >= 0) {
        select(item);
        dbl = doubleClick;                                  // vt[10] sub_46BCD0: clique duplo = DETAILS
    }
    return true;
}

bool LabsPane::detailsEnabled() const {
    return m_open && m_mode != 2 && m_chosen;
}

} // namespace op2
