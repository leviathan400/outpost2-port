#include "vehicle_list_pane.h"

#include <algorithm>
#include <string>

#include "produce_pane.h"
#include "../gfx/ui_font.h"
#include "../game/building_sheet.h"
#include "../game/player.h"
#include "../game/research.h"
#include "../game/tethys_game.h"
#include "../game/unit.h"

namespace op2 {

static uint32_t rgb555(uint16_t c) {
    const uint32_t r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void VehicleListPane::open() {                              // sub_46B180
    m_open = true;
    m_sel = -1;
    m_top = 0;
    refresh();
}

void VehicleListPane::refresh() {                           // sub_46B1E0
    const int me = TethysGame::localPlayer();
    for (int& c : m_count) c = 0;
    for (size_t i = 1; i < g_units.size(); ++i) {           // lista de veículos do jogador (dword_56EEF8)
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD) || !(r.flags & UNIT_VEHICLE) || (r.owner & 0x0F) != me) continue;
        if (r.type > 0 && r.type <= 0x10) ++m_count[r.type];
    }
    m_types.clear();
    for (int t = 1; t <= 0x0F; ++t)                         // off_4E1344..off_4E137C
        if (researchHas(me, researchTopicIndex(t)) || m_count[t]) m_types.push_back(t);
    m_lastTick = static_cast<int>(TethysGame::tick());
    if (m_sel >= static_cast<int>(m_types.size())) m_sel = -1;
    if (m_top >= static_cast<int>(m_types.size())) m_top = 0;
}

void VehicleListPane::update() {                            // sub_46B120
    if (!m_open) return;
    const int t = static_cast<int>(TethysGame::tick());
    if (t != m_lastTick && (t & 3) == 1) refresh();
}

ListBoxLayout VehicleListPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x0A, 0, 0x12);           // sub_459940(.., 1, 0, 0xA, 0, 0x12)
}

void VehicleListPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   "Vehicle List (Alt+V)", 0xFF00FF00u, true, kMarkupLine);   // sub_459B60 (unk_578F08)
    drawListBoxFrame(fb, fbW, fbH, L);
    auto fill = [&](int x, int y, int w, int h, uint32_t c) {
        for (int yy = std::max(0, y); yy < std::min(fbH, y + h); ++yy)
            for (int xx = std::max(0, x); xx < std::min(fbW, x + w); ++xx) fb[yy * fbW + xx] = c;
    };
    auto frame = [&](int x, int y, int w, int h, uint32_t c) {
        fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c); fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c);
    };
    for (int i = 0; i < L.rows && m_top + i < static_cast<int>(m_types.size()); ++i) {
        const int t = m_types[m_top + i];
        const int y = L.list.t + 2 + i * L.lineH;
        if (m_top + i == m_sel) {
            fill(L.list.l + 2, y, L.list.r - L.list.l - 4, L.lineH, rgb555(0x1CE7));
            frame(L.list.l + 2, y, L.list.r - L.list.l - 4, L.lineH, rgb555(0x3DEF));
        }
        const std::string row = std::string(unitDisplayName(t)) + "s<R>" + std::to_string(m_count[t]);   // "%ss<R>%i"
        drawMarkupText(fb, fbW, fbH, PaneRect{L.list.l + 4, y + 1, L.list.r - 4, y + L.lineH}, row, 0xFF00FF00u, false, kMarkupLine);
    }
    const bool canUp = m_top > 0, canDown = m_top + L.rows < static_cast<int>(m_types.size());
    drawListBoxArrows(fb, fbW, fbH, L, pane, canUp, canDown, mx, my);
}

bool VehicleListPane::click(const PaneRect& pane, int x, int y) {
    if (!m_open) return false;
    const ListBoxLayout L = layout(pane);
    auto in = [&](const PaneRect& r) { return x >= r.l && x < r.r && y >= r.t && y < r.b; };
    if (in(L.up)) { m_top = std::max(0, m_top - L.rows); return true; }
    if (in(L.down)) {
        if (m_top + L.rows < static_cast<int>(m_types.size())) m_top += L.rows;
        return true;
    }
    if (!in(L.list)) return false;
    const int i = (y - L.list.t - 2) / L.lineH;
    if (i >= 0 && i < L.rows && m_top + i < static_cast<int>(m_types.size())) m_sel = m_top + i;   // sub_46B340
    return true;
}

bool VehicleListPane::jumpEnabled() const {                 // sub_46B340
    return m_open && m_sel >= 0 && m_sel < static_cast<int>(m_types.size()) && m_count[m_types[m_sel]] > 0;
}

int VehicleListPane::jump(int& px, int& py) const {         // sub_46B3D0
    if (m_sel < 0 || m_sel >= static_cast<int>(m_types.size())) return 0;
    const int type = m_types[m_sel], me = TethysGame::localPlayer();
    int best = 0, bestSerial = 0x270F;
    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD) || !(r.flags & UNIT_VEHICLE) || (r.owner & 0x0F) != me) continue;
        if (r.type != type) continue;
        if (static_cast<int8_t>(r.serial) < bestSerial) { bestSerial = static_cast<int8_t>(r.serial); best = static_cast<int>(i); }
    }
    if (!best) return 0;
    px = unitOX(g_units[best]); py = unitOY(g_units[best]);
    return best;
}

} // namespace op2
