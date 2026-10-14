#include "comms_pane.h"

#include <string>

#include "../game/messages.h"
#include "../game/tethys_game.h"

namespace op2 {

// Item i do provedor: a i-ésima mensagem mais nova (sub_466630).
static const GameMessage& itemMessage(int i) { return messageAt(messageCount() - 1 - i); }

void CommsPane::open() {                                    // sub_466920 (modo 0)
    m_open = true;
    m_list.reset();
    m_seen = messageSerial();
}

void CommsPane::update() {                                  // sub_4668C0
    if (!m_open) return;
    if ((TethysGame::tick() & 3) != 1 || m_seen >= messageSerial()) return;
    m_seen = messageSerial();
    // sub_430F30: a mensagem nova entra no topo; a escolhida desce uma linha.
    if (m_list.sel != -1) m_list.sel = (m_list.sel + 1 < messageCount()) ? m_list.sel + 1 : -1;
}

ListBoxLayout CommsPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x19, 0, 0x12);           // sub_459940(dword_5670F4, 1, 0, 0x19, 0, 0x12)
}

std::vector<std::string> CommsPane::items() const {         // sub_466630: "%s" / "<C2>%s"
    std::vector<std::string> out;
    for (int i = 0; i < messageCount(); ++i) {
        const GameMessage& m = itemMessage(i);
        out.push_back((m.x == -2 && m.y == -1) ? m.text : "<C2>" + m.text);
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
    m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

bool CommsPane::click(const PaneRect& pane, int x, int y) {
    if (!m_open) return false;
    int item;
    return m_list.click(layout(pane), items(), x, y, item);  // item escolhido: sub_466970
}

bool CommsPane::jumpEnabled() const {                       // sub_466970: y != −1
    return m_open && m_list.sel >= 0 && m_list.sel < messageCount() && itemMessage(m_list.sel).y != -1;
}

bool CommsPane::jump(int& px, int& py) const {              // sub_466A00 → sub_438EC0
    if (!jumpEnabled()) return false;
    const GameMessage& m = itemMessage(m_list.sel);
    px = m.x; py = m.y;
    return true;
}

} // namespace op2
