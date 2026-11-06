#include "research_pane.h"
#include "../audio/sound.h"

#include <algorithm>
#include <cstdio>

#include "produce_pane.h"
#include "../gfx/art_blit.h"
#include "../gfx/artfile.h"
#include "../gfx/font.h"
#include "../gfx/ui_font.h"
#include "../game/colony.h"
#include "../game/research.h"
#include "../game/research_lab.h"
#include "../game/building_sheet.h"
#include "../game/structure.h"
#include "../game/tethys_game.h"
#include "../game/unit.h"

namespace op2 {

static ListBoxLayout topicsLayout(const PaneRect& p) { return listBoxLayout(p, 0, 0x14, 0, 0x12); }
static ListBoxLayout teaserLayout(const PaneRect& p) { return listBoxLayout(p, 0, 0x64, 0, 0x12); }

bool ResearchPane::researching() const {
    return m_lab > 0 && m_lab < static_cast<int>(g_units.size()) && g_units[m_lab].action == 0x16;
}

void ResearchPane::open(int labId, int mode) {           // sub_46C210
    m_open = true; m_lab = labId; m_mode = mode; m_top = 0;
    const UnitRecord& r = g_units[labId];
    if (mode == 0) {
        m_topics = labTopics(labId);                     // sub_46C390 (modo 0)
        m_topic = 0;
        return;
    }
    // Modo 1: cientistas já no projeto ([+0x64]); tópico atual se pesquisando.
    m_sciCur = r.extraScientists;
    if (r.action == 0x16) m_topic = r.labTopic;
    const int avail = g_colony[r.owner & 0x0F].scientistsLeft;   // dword_56E384 (+0xA8)
    const Tech* t = techAt(m_topic);
    const int maxSci = t ? t->maxScientists : 0;
    m_sci = std::min(maxSci, r.action == 0x16 ? avail + m_sciCur : avail);
}

void ResearchPane::update(const std::vector<int>& sel) {
    if (!m_open) return;
    if (sel.size() != 1 || sel[0] != m_lab || g_units[m_lab].id == -1 || (g_units[m_lab].flags & UNIT_DEAD))
        m_open = false;
}

std::vector<std::string> ResearchPane::teaserLines(int width) const {   // sub_46C390 (modo 1)
    std::vector<std::string> out;
    const Tech* t = techAt(m_topic);
    if (!t) return out;
    std::string line, word;
    auto flush = [&]() {
        if (word.empty()) return;
        const std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && UiFont::width(cand) > width) { out.push_back(line); line = word; }
        else line = cand;
        word.clear();
    };
    for (char ch : t->teaser) {
        if (ch == ' ') flush();
        else if (ch == '\n' || ch == '\r') { flush(); if (!line.empty()) out.push_back(line); line.clear(); }
        else word += ch;
    }
    flush();
    if (!line.empty()) out.push_back(line);
    return out;
}

PaneRect ResearchPane::lessRect(int winL, int winT) const { return PaneRect{winL + 9, winT + 0x7B, winL + 9 + 87, winT + 0x7B + 25}; }
PaneRect ResearchPane::moreRect(int winL, int winT) const { return PaneRect{winL + 9 + 87, winT + 0x7B, winL + 9 + 174, winT + 0x7B + 25}; }

void ResearchPane::draw(uint32_t* fb, int fbW, int fbH, int winL, int winT, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const std::string labName = unitTitle(g_units[m_lab]);
    if (m_mode == 0) {
        const ListBoxLayout L = topicsLayout(pane);
        drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                       labName + "\nAvailable Topics", 0xFF00FF00u, true, kMarkupLine);
        drawListBoxFrame(fb, fbW, fbH, L);
        auto rgb555 = [](uint16_t c) {
            return 0xFF000000u | (uint32_t(((c >> 10) & 31) << 3) << 16) | (uint32_t(((c >> 5) & 31) << 3) << 8) | uint32_t((c & 31) << 3);
        };
        for (int i = 0; i < L.rows && m_top + i < static_cast<int>(m_topics.size()); ++i) {
            const int topic = m_topics[m_top + i];
            const int y = L.list.t + 2 + i * L.lineH;
            if (topic == m_topic) {
                for (int yy = y; yy < y + L.lineH; ++yy)
                    for (int xx = L.list.l + 2; xx < L.list.r - 2; ++xx) {
                        const bool edge = yy == y || yy == y + L.lineH - 1 || xx == L.list.l + 2 || xx == L.list.r - 3;
                        fb[yy * fbW + xx] = rgb555(edge ? 0x3DEF : 0x1CE7);
                    }
            }
            std::string name = techAt(topic) ? techAt(topic)->name : "";
            while (!name.empty() && UiFont::width(name) > L.list.r - L.list.l - 8) name.pop_back();
            UiFont::draw(fb, fbW, fbH, name, L.list.l + 4, y + 1, 0xFF00FF00u, false);
        }
        drawListBoxArrows(fb, fbW, fbH, L, pane, m_top > 0, m_top + L.rows < static_cast<int>(m_topics.size()), mx, my);
        return;
    }
    // Modo 1.
    const ListBoxLayout L = teaserLayout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, pane.t + 0x28},
                   labName + "\nAssign Scientists", 0xFF00FF00u, true);
    const Tech* t = techAt(m_topic);
    const int avail = g_colony[g_units[m_lab].owner & 0x0F].scientistsLeft;
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%s %s<R>%i\n<L>%s<R>%i\n<L>%s<R>%i", "Scientists", "Available", avail,
                  "Assigned To This Project", m_sci, "Maximum Project Staff", t ? t->maxScientists : 0);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 0x28, pane.r - 0xE, L.list.t}, buf, 0xFF00FF00u);
    // LESS / MORE (sub_46C520: arte 0x98, x = 9 / 9 + largura, y = 0x7B).
    auto textBtn = [&](const PaneRect& r, const char* label) {
        const int anim = 0x98 - 0x65;
        const bool over = mx >= r.l && mx < r.r && my >= r.t && my < r.b;
        if (g_art.ok() && anim < g_art.numAnimations())
            blitArtFrame(fb, fbW, fbH, anim, over ? 1 : 0, r.l, r.t, -1, pane.l, pane.t, pane.r, pane.b);
        UiFont::drawCentered(fb, fbW, fbH, label, (r.l + r.r) / 2, UiFont::centeredY(r.t, r.b), over ? 0xFFFF7050 : 0xFFE04030, false);
    };
    textBtn(lessRect(winL, winT), "LESS");
    textBtn(moreRect(winL, winT), "MORE");
    drawListBoxFrame(fb, fbW, fbH, L);
    const std::vector<std::string> lines = teaserLines(L.list.r - L.list.l - 8);
    for (int i = 0; i < L.rows && m_top + i < static_cast<int>(lines.size()); ++i)
        UiFont::draw(fb, fbW, fbH, lines[m_top + i], L.list.l + 4, L.list.t + 3 + i * L.lineH, 0xFF00FF00u, false,
                     L.list.l + 1, L.list.t + 1, L.list.r - 1, L.list.b - 1);   // recortado pela lista
    drawListBoxArrows(fb, fbW, fbH, L, pane, m_top > 0, m_top + L.rows < static_cast<int>(lines.size()), mx, my);
}

void ResearchPane::less() {                              // sub_46C990 (−1)
    if (m_sci > 1) --m_sci;
}
void ResearchPane::more() {                              // sub_46C990 (+1)
    const Tech* t = techAt(m_topic);
    const int avail = g_colony[g_units[m_lab].owner & 0x0F].scientistsLeft;
    const int cap = std::min(t ? t->maxScientists : 0, avail + (researching() ? m_sciCur : 0));
    if (m_sci < cap) ++m_sci;
}
void ResearchPane::start() {                             // sub_457610
    if (m_topic > 0 && m_sci > 0) sendStartResearch(m_lab, m_topic, m_sci);
    m_open = false;
}

bool ResearchPane::click(int winL, int winT, const PaneRect& pane, int x, int y, bool dbl, bool& dblAssign) {
    dblAssign = false;
    if (!m_open) return false;
    auto in = [&](const PaneRect& r) { return x >= r.l && x < r.r && y >= r.t && y < r.b; };
    if (m_mode == 1) {
        if (in(lessRect(winL, winT))) { soundPlay(0x57); less(); return true; }
        if (in(moreRect(winL, winT))) { soundPlay(0x57); more(); return true; }
        const ListBoxLayout L = teaserLayout(pane);
        const int n = static_cast<int>(teaserLines(L.list.r - L.list.l - 8).size());
        if (in(L.up)) { m_top = std::max(0, m_top - L.rows); return true; }
        if (in(L.down)) { if (m_top + L.rows < n) m_top += L.rows; return true; }
        return in(L.list);
    }
    const ListBoxLayout L = topicsLayout(pane);
    if (in(L.up)) { m_top = std::max(0, m_top - L.rows); return true; }
    if (in(L.down)) { if (m_top + L.rows < static_cast<int>(m_topics.size())) m_top += L.rows; return true; }
    if (!in(L.list)) return false;
    const int i = (y - L.list.t - 2) / L.lineH;
    if (i >= 0 && i < L.rows && m_top + i < static_cast<int>(m_topics.size())) {
        m_topic = m_topics[m_top + i];                   // sub_46C830
        dblAssign = dbl;                                 // sub_46C800
    }
    return true;
}

// ---- University ------------------------------------------------------------

int StudentsPane::maxClass() const {
    const UnitRecord& r = g_units[m_univ];
    const StructSheetDesc* d = structSheet(r.type);
    return d ? d->player[(r.owner >> 4) & 0x0F].productionCapacity : 0;   // bloco+0x34
}

void StudentsPane::open(int univId) {                   // sub_46E020
    m_open = true; m_univ = univId;
    const UnitRecord& r = g_units[univId];
    if (r.action != 0x17) m_n = std::min(g_colony[r.owner & 0x0F].workersLeft, maxClass());   // dword_56E380
}

void StudentsPane::update(const std::vector<int>& sel) {
    if (!m_open) return;
    if (sel.size() != 1 || sel[0] != m_univ || g_units[m_univ].id == -1 || (g_units[m_univ].flags & UNIT_DEAD))
        m_open = false;
}

void StudentsPane::more() {
    const int cap = std::min(maxClass(), g_colony[g_units[m_univ].owner & 0x0F].workersLeft);
    if (m_n < cap) ++m_n;
}

void StudentsPane::start() {
    if (m_n > 0) sendTrain(m_univ, m_n);
    m_open = false;
}

void StudentsPane::draw(uint32_t* fb, int fbW, int fbH, int winL, int winT, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const UnitRecord& r = g_units[m_univ];
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 1, pane.r - 0xD, pane.t + 0x28},
                   unitTitle(r) + "\nAssign Students To Class", 0xFF00FF00u, true);
    const int avail = g_colony[r.owner & 0x0F].workersLeft;
    const int n = std::min(m_n, avail);                  // sub_46E220 limita ao disponível
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%s %s<R>%i" "\n" "<L>%s<R>%i" "\n" "<L>%s<R>%i", "Workers", "Available", avail,
                  "Assigned To Training", n, "Maximum Class Size", maxClass());
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 1 + 0x28, pane.r - 0xD, pane.t + 0x87}, buf, 0xFF00FF00u);
    auto textBtn = [&](int x, const char* label) {
        const PaneRect b{winL + x, winT + 0x91, winL + x + 87, winT + 0x91 + 25};
        const int anim = 0x98 - 0x65;
        const bool over = mx >= b.l && mx < b.r && my >= b.t && my < b.b;
        if (g_art.ok() && anim < g_art.numAnimations())
            blitArtFrame(fb, fbW, fbH, anim, over ? 1 : 0, b.l, b.t, -1, pane.l, pane.t, pane.r, pane.b);
        UiFont::drawCentered(fb, fbW, fbH, label, (b.l + b.r) / 2, UiFont::centeredY(b.t, b.b), over ? 0xFFFF7050 : 0xFFE04030, false);
    };
    textBtn(9, "LESS");
    textBtn(9 + 87, "MORE");
}

bool StudentsPane::click(int winL, int winT, int x, int y) {
    if (!m_open) return false;
    if (y < winT + 0x91 || y >= winT + 0x91 + 25) return false;
    if (x >= winL + 9 && x < winL + 96) { soundPlay(0x57); less(); return true; }
    if (x >= winL + 96 && x < winL + 183) { soundPlay(0x57); more(); return true; }
    return false;
}

// ---- Confirmação -------------------------------------------------------------

// sub_45C1A0: a confirmacao se fecha sozinha quando a selecao deixa de ser a
// unidade que a abriu -- MENOS quando quem a abriu foi o painel de Game
// Options (`cmp eax, offset unk_565AC8`), porque a tela END MISSION nao
// depende de selecao. Era por isso que, sem nada selecionado, os itens dessa
// tela nao faziam NADA: a confirmacao fechava no quadro seguinte.
void ConfirmPane::update(const std::vector<int>& sel) {
    if (!m_open || m_fromOptions) return;
    if (sel.size() != 1 || sel[0] != m_unit) m_open = false;
}

void ConfirmPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane) const {   // sub_45C0B0
    if (!m_open) return;
    // Quebra por palavras (sub_415EC0, centrado) na largura W − 0xE − 0xC.
    const int width = pane.r - pane.l - 0xE - 0xC - 0xD;
    std::string out, line, word;
    auto flush = [&]() {
        if (word.empty()) return;
        const std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && UiFont::width(cand) > width) { out += "<X>" + line + "\n"; line = word; }
        else line = cand;
        word.clear();
    };
    for (char ch : m_msg) { if (ch == ' ') flush(); else word += ch; }
    flush();
    out += "<X>" + line;
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 0xD, pane.r - 0xE, pane.b}, out, 0xFFFF0000u);
}

} // namespace op2
