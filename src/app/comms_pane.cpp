#include "comms_pane.h"

#include <algorithm>
#include <string>

#include "../gfx/ui_font.h"
#include "../game/messages.h"
#include "../game/tethys_game.h"

namespace op2 {

static uint32_t rgb555(uint16_t c) {
    const uint32_t r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// Item i do provedor: a i-ésima mensagem mais nova (sub_466630).
static const GameMessage& itemMessage(int i) { return messageAt(messageCount() - 1 - i); }

void CommsPane::open() {                                    // sub_466920 (modo 0)
    m_open = true;
    m_sel = -1;
    m_top = 0;
    m_seen = messageSerial();
}

void CommsPane::update() {                                  // sub_4668C0
    if (!m_open) return;
    if ((TethysGame::tick() & 3) != 1 || m_seen >= messageSerial()) return;
    m_seen = messageSerial();
    // sub_430F30: a mensagem nova entra no topo; a escolhida desce uma linha.
    if (m_sel != -1) m_sel = (m_sel + 1 < messageCount()) ? m_sel + 1 : -1;
}

ListBoxLayout CommsPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x19, 0, 0x12);           // sub_459940(dword_5670F4, 1, 0, 0x19, 0, 0x12)
}

// Linhas da lista: cada mensagem quebrada por palavras na largura (sub_416650),
// com as tags do começo repetidas em cada linha.
std::vector<CommsPane::Line> CommsPane::lines(int width) const {
    std::vector<Line> out;
    const int n = messageCount();
    for (int i = 0; i < n; ++i) {
        const GameMessage& m = itemMessage(i);
        std::string tag = (m.x == -2 && m.y == -1) ? "" : "<C2>";   // "%s" / "<C2>%s"
        std::string text = m.text;
        if (text.compare(0, 3, "<N>") == 0) { tag += "<N>"; text = text.substr(3); }
        std::string cur, word;
        for (size_t k = 0; k <= text.size(); ++k) {
            if (k == text.size() || text[k] == ' ') {
                const std::string cand = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && UiFont::width(cand) > width) { out.push_back({i, tag + cur}); cur = word; }
                else cur = cand;
                word.clear();
            } else {
                word += text[k];
            }
        }
        out.push_back({i, tag + cur});
    }
    return out;
}

void CommsPane::draw(uint32_t* fb, int fbW, int fbH, int winX, int winY, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   "Communications: Messages", 0xFF00FF00u, true);     // sub_459B60
    // "%i: %s" (sub_439400 = tick/100, "Current Mark") — retângulo da janela do quadro.
    const std::string mark = std::to_string(TethysGame::tick() / 100) + ": Current Mark";
    drawMarkupText(fb, fbW, fbH, PaneRect{winX + 0xA, winY + 0x37, pane.r - 0xC, winY + 0x37 + 0xF},
                   mark, 0xFF00FF00u);
    drawListBoxFrame(fb, fbW, fbH, L);
    auto fill = [&](int x, int y, int w, int h, uint32_t c) {
        for (int yy = std::max(0, y); yy < std::min(fbH, y + h); ++yy)
            for (int xx = std::max(0, x); xx < std::min(fbW, x + w); ++xx) fb[yy * fbW + xx] = c;
    };
    const std::vector<Line> ls = lines(L.list.r - L.list.l - 8);
    const int top = std::min(m_top, std::max(0, static_cast<int>(ls.size()) - 1));
    for (int i = 0; i < L.rows && top + i < static_cast<int>(ls.size()); ++i) {
        const Line& ln = ls[top + i];
        const int y = L.list.t + 2 + i * L.lineH;
        if (ln.item == m_sel) {
            const bool first = top + i == 0 || ls[top + i - 1].item != ln.item;
            const bool last = top + i + 1 >= static_cast<int>(ls.size()) || ls[top + i + 1].item != ln.item;
            const int x = L.list.l + 2, w = L.list.r - L.list.l - 4;
            fill(x, y, w, L.lineH, rgb555(0x1CE7));
            fill(x, y, 1, L.lineH, rgb555(0x3DEF)); fill(x + w - 1, y, 1, L.lineH, rgb555(0x3DEF));
            if (first) fill(x, y, w, 1, rgb555(0x3DEF));
            if (last) fill(x, y + L.lineH - 1, w, 1, rgb555(0x3DEF));
        }
        drawMarkupText(fb, fbW, fbH, PaneRect{L.list.l + 4, y + 1, L.list.r - 4, y + L.lineH}, ln.text, 0xFF00FF00u);
    }
    drawListBoxArrows(fb, fbW, fbH, L, pane, top > 0, top + L.rows < static_cast<int>(ls.size()), mx, my);
}

bool CommsPane::click(const PaneRect& pane, int x, int y) {
    if (!m_open) return false;
    const ListBoxLayout L = layout(pane);
    auto in = [&](const PaneRect& r) { return x >= r.l && x < r.r && y >= r.t && y < r.b; };
    const std::vector<Line> ls = lines(L.list.r - L.list.l - 8);
    const int n = static_cast<int>(ls.size());
    if (in(L.up)) { m_top = std::max(0, m_top - L.rows); return true; }
    if (in(L.down)) { if (m_top + L.rows < n) m_top += L.rows; return true; }
    if (!in(L.list)) return false;
    const int i = (y - L.list.t - 2) / L.lineH;
    if (i >= 0 && i < L.rows && m_top + i < n) m_sel = ls[m_top + i].item;   // sub_466970
    return true;
}

bool CommsPane::jumpEnabled() const {                       // sub_466970: y != −1
    return m_open && m_sel >= 0 && m_sel < messageCount() && itemMessage(m_sel).y != -1;
}

bool CommsPane::jump(int& px, int& py) const {              // sub_466A00 → sub_438EC0
    if (!jumpEnabled()) return false;
    const GameMessage& m = itemMessage(m_sel);
    px = m.x; py = m.y;
    return true;
}

} // namespace op2
