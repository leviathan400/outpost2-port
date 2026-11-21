#include "objectives_pane.h"

#include <algorithm>
#include <string>
#include <vector>

#include "produce_pane.h"
#include "../gfx/font.h"
#include "../gfx/ui_font.h"
#include "../game/trigger.h"

namespace op2 {

static bool s_showCompleted = false;            // dword_57563C
bool objectivesShowCompleted() { return s_showCompleted; }
void objectivesSetShowCompleted(bool on) { s_showCompleted = on; }

static ListBoxLayout objLayout(const PaneRect& pane) { return listBoxLayout(pane, 0, 0xA, 0, 0x12); }

// Linhas da lista: cada objetivo pendente quebrado por palavras na largura da
// lista (sub_416650), com a tag de cor repetida em cada linha.
static std::vector<std::string> objectiveLines(int width, std::vector<std::string>* owners = nullptr) {
    std::vector<std::string> out;
    // owners: o objetivo de cada linha (a linha em branco entre dois e do de cima).
    auto dono = [&](const std::string& d) { if (owners) while (owners->size() < out.size()) owners->push_back(d); };
    for (const std::string& line : missionObjectives()) {
        const std::string tag = line.substr(0, 4), text = line.substr(4);
        if (tag == "<C0>" && !objectivesShowCompleted()) continue;   // cumprido: fora se dword_57563C == 0
        // O texto pode trazer quebras de linha do proprio script da missao --
        // o demo de atracao, por exemplo, manda "Outpost 2 demo mode.", duas
        // quebras e "Press any key to return to the Main Menu.". Elas contam
        // como fim de linha; dentro de cada pedaco a quebra e por palavras, na
        // largura da lista. Sem isto a quebra vira parte de uma "palavra" e o
        // texto sai fora de ordem na tela.
        size_t ini = 0;
        while (ini <= text.size()) {
            size_t fim = text.find('\n', ini);
            if (fim == std::string::npos) fim = text.size();
            const std::string pedaco = text.substr(ini, fim - ini);
            std::string cur, word;
            for (size_t i = 0; i <= pedaco.size(); ++i) {
                if (i == pedaco.size() || pedaco[i] == ' ') {
                    const std::string cand = cur.empty() ? word : cur + " " + word;
                    if (!cur.empty() && UiFont::width(cand) > width) { out.push_back(tag + cur); cur = word; }
                    else cur = cand;
                    word.clear();
                } else {
                    word += pedaco[i];
                }
            }
            if (!cur.empty()) out.push_back(tag + cur);
            else if (fim < text.size()) out.push_back("");   // linha em branco pedida pelo script
            if (fim == text.size()) break;
            ini = fim + 1;
        }
        out.push_back("");                               // espaço entre objetivos
        dono(line);
    }
    if (!out.empty()) out.pop_back();
    if (owners) owners->resize(out.size());
    return out;
}

void ObjectivesPane::reanchor(const std::vector<std::string>& lines, const std::vector<std::string>& owners, int rows) const {
    int& top = const_cast<int&>(m_top);
    const int n = static_cast<int>(lines.size());
    if (lines != m_lastLines) {
        if (!m_lastLines.empty()) {                      // a lista mudou: re-ancora
            int novo = 0;                                // o item sumiu: o comeco
            for (int i = 0; i < n; ++i)
                if (!m_anchor.empty() && owners[i] == m_anchor) {
                    int fim = i;
                    while (fim + 1 < n && owners[fim + 1] == m_anchor) ++fim;
                    novo = std::min(i + m_anchorOff, fim);
                    break;
                }
            top = novo;
        }
        m_lastLines = lines;
    }
    // CORRECAO (5.54): a janela nunca fica com o fim vazio enquanto houver
    // objetivos acima -- o topo para em (total - linhas da janela). So com a
    // re-ancoragem, o objetivo do topo podia ser o ultimo da lista e a janela
    // mostrava uma linha so, com os restantes fora da vista.
    top = std::max(0, std::min(top, std::max(0, n - std::max(1, rows))));
    // A ancora de agora: o objetivo da linha do topo e a linha dentro dele.
    if (top < n) {
        m_anchor = owners[top];
        int ini = top;
        while (ini > 0 && owners[ini - 1] == m_anchor) --ini;
        m_anchorOff = top - ini;
    } else {
        m_anchor.clear();
        m_anchorOff = 0;
    }
}

int ObjectivesPane::debugRows(const PaneRect& pane) const { return objLayout(pane).rows; }
int ObjectivesPane::debugVisible(const PaneRect& pane) const {
    const ListBoxLayout L = objLayout(pane);
    const std::vector<std::string> lines = objectiveLines(L.list.r - L.list.l - 8);
    int v = 0;
    for (int i = 0; i < L.rows && m_top + i < static_cast<int>(lines.size()); ++i) if (!lines[m_top + i].empty()) ++v;
    return v;
}

std::vector<std::string> objectivesDebugLines(int width) { return objectiveLines(width); }

void ObjectivesPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    const ListBoxLayout L = objLayout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   "Mission Objectives (Alt+O)", 0xFF00FF00u, true, kMarkupLine);           // sub_459B60 (verde)
    drawListBoxFrame(fb, fbW, fbH, L);
    std::vector<std::string> owners;
    const std::vector<std::string> lines = objectiveLines(L.list.r - L.list.l - 8, &owners);
    reanchor(lines, owners, L.rows);                     // sub_430CE0 (5.54)
    const int top = m_top;
    for (int i = 0; i < L.rows && top + i < static_cast<int>(lines.size()); ++i)
        drawMarkupText(fb, fbW, fbH, PaneRect{L.list.l + 4, L.list.t + 2 + i * L.lineH, L.list.r - 4, L.list.b},
                       lines[top + i], 0xFF00FF00u);
    (void)top;
    drawListBoxScroll(fb, fbW, fbH, L, pane, const_cast<int&>(m_top), static_cast<int>(lines.size()), mx, my);   // (5.50)
}

bool ObjectivesPane::click(const PaneRect& pane, int x, int y) {
    const ListBoxLayout L = objLayout(pane);
    auto in = [&](const PaneRect& r) { return x >= r.l && x < r.r && y >= r.t && y < r.b; };
    const int n = static_cast<int>(objectiveLines(L.list.r - L.list.l - 8).size());
    if (in(L.up)) { m_top = std::max(0, m_top - L.rows); return true; }
    if (in(L.down)) { if (m_top + L.rows < n) m_top += L.rows; return true; }
    return in(L.list);
}

} // namespace op2
