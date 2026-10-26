#include "space_pane.h"

#include "../game/building_sheet.h"
#include "../game/player.h"
#include "../game/research.h"
#include "../game/tethys_game.h"

namespace op2 {

static bool typeResearched(int type) {                      // sub_472C50(jogador, desc+0x1E4)
    return researchHas(TethysGame::localPlayer(), researchTopicIndex(type));
}

bool SpacePane::starshipAvailable() { return typeResearched(0x37); }

void SpacePane::open(int mode) {                            // sub_46CD00
    m_open = true;
    m_mode = mode;
    m_list.reset();
    refresh();
}

void SpacePane::refresh() {                                 // sub_46CD50
    const int me = TethysGame::localPlayer();
    m_ids.clear();
    if (m_mode == 0) {
        m_ids.push_back(0);
        for (int i = 0; i < playerSpaceCount(me, 0x58); ++i) m_ids.push_back(0x58);
        for (int i = 0; i < playerSpaceCount(me, 0x59); ++i) m_ids.push_back(0x59);
    } else {
        for (int k = 0; k < 10; ++k)
            if (typeResearched(0x5A + k) || playerSpaceHas(me, 0x5A + k)) m_ids.push_back(k);
        if ((g_playerData[me].spaceBits & 0x7FE000u) == 0x7FE000u) {   // sub_4907E0
            for (int k = 10; k < 15; ++k) {
                const int topic = researchTopicIndex(0x5A + k);
                if (topic <= 0 || researchHas(me, topic)) m_ids.push_back(k);
            }
        }
    }
    m_lastTick = static_cast<int>(TethysGame::tick());
}

void SpacePane::update() {                                  // loc_46CCE0
    if (!m_open) return;
    const int t = static_cast<int>(TethysGame::tick());
    if (t != m_lastTick && (t & 3) == 1) refresh();
}

std::vector<std::string> SpacePane::items() const {         // sub_46D040
    const int me = TethysGame::localPlayer();
    std::vector<std::string> out;
    for (int v : m_ids) {
        if (m_mode == 0) {
            if (v == 0) out.push_back(std::string("Communications Satellite") + ": " + "<C2>\n\tNot Operational");
            else out.push_back(std::string(unitDisplayName(v)) + ": " + "\n\tOperational");
        } else {
            const bool orbit = playerSpaceHas(me, 0x5A + v) != 0;
            out.push_back(std::string(orbit ? "<C0>" : "<C2>") + unitDisplayName(0x5A + v) + "\n\t" +
                          (orbit ? "In Orbit" : "Researched"));
        }
    }
    return out;
}

void SpacePane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   m_mode == 0 ? "Space Program: Satellites" : "Space Program: Starship",
                   0xFF00FF00u, true, kMarkupLine);                                 // sub_46CF40
    m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

bool SpacePane::click(const PaneRect& pane, int x, int y) {
    if (!m_open) return false;
    int item;
    const bool hit = m_list.click(layout(pane), items(), x, y, item);
    m_list.sel = -1;                                        // lista sem escolha
    return hit;
}

} // namespace op2
