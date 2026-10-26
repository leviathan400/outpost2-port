#include "resources_pane.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include "../game/colony.h"
#include "../game/mining.h"
#include "../game/player.h"
#include "../game/research.h"
#include "../game/structure.h"
#include "../game/tethys_game.h"
#include "../game/unit.h"

namespace op2 {

static const uint32_t kGreen = 0xFF00FF00u;

// sub_472C50(jogador, desc+0x1E4): o tópico que libera o tipo foi pesquisado.
static bool typeResearched(int type) {
    const int me = TethysGame::localPlayer();
    return researchHas(me, researchTopicIndex(type));
}

bool ResourcesPane::rareAvailable() { return typeResearched(0x16); }

void ResourcesPane::open(int mode) {                        // sub_468AC0
    m_open = true;
    m_mode = mode;
    m_list.reset();
    m_chosen = false;                                       // sub_469490: DETAILS começa desabilitado
    refresh();
}

void ResourcesPane::refresh() {                             // sub_468B60
    const int me = TethysGame::localPlayer();
    m_ids.clear();
    auto groupOf = [&](std::initializer_list<int> types) {
        for (int id : colonyStructures(me)) {
            const int t = g_units[id].type;
            if (std::find(types.begin(), types.end(), t) != types.end()) m_ids.push_back(id);
        }
    };
    switch (m_mode) {
        case 0:
            m_ids = {1, 2, 3, 4, 5};
            if (rareAvailable()) m_ids.push_back(6);
            break;
        case 2: {                                           // fatores de moral
            m_ids = {1, 2, 3, 4, 5};
            const bool eden = g_playerData[me].isEden != 0;
            if ((!eden && typeResearched(0x1B)) || g_colony[me].stat(ST_FORUM_CAP)) m_ids.push_back(6);
            else if (typeResearched(0x25)) m_ids.push_back(7);
            if (typeResearched(0x22)) m_ids.push_back(8);
            if (typeResearched(0x23)) m_ids.push_back(9);
            if (typeResearched(0x26)) m_ids.push_back(10);
            if (typeResearched(0x39)) m_ids.push_back(11);
            if (typeResearched(0x4E) || typeResearched(0x4C) || typeResearched(0x4D) ||
                typeResearched(0x4F) || typeResearched(0x50)) m_ids.push_back(12);
            if (typeResearched(0x28)) m_ids.push_back(13);
            break;
        }
        case 7:  groupOf({0x23, 0x22, 0x26}); break;
        case 8:  groupOf({0x1B, 0x2E, 0x25, 0x1E, 0x35, 0x34}); break;
        case 9:  groupOf({0x3A, 0x2C, 0x24, 0x1D, 0x1C}); break;
        case 10: groupOf({0x27}); break;
        case 11: groupOf({0x15, 0x36, 0x19}); break;
        case 12: groupOf({0x16, 0x38, 0x1A, 0x2A}); break;
        default: break;
    }
    if (m_list.sel >= static_cast<int>(m_ids.size())) m_list.sel = -1;
    m_lastTick = static_cast<int>(TethysGame::tick());
}

void ResourcesPane::update() {                              // sub_468A20
    if (!m_open) return;
    const int t = static_cast<int>(TethysGame::tick());
    if (t != m_lastTick && (t & 3) == 1) refresh();
}

ListBoxLayout ResourcesPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, m_mode == 2 ? 0x28 : 0x0A, 0, 0x12);   // sub_468AC0
}

static std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

std::vector<std::string> ResourcesPane::items() const {     // sub_46A600
    const int me = TethysGame::localPlayer();
    const PlayerData& pd = g_playerData[me];
    const ColonyState& c = g_colony[me];
    std::vector<std::string> out;
    for (int v : m_ids) {
        if (m_mode == 0) {
            switch (v) {
                case 1:
                    out.push_back(fmt("%s<R>%i\n<L>\t%s %s<R>%i\n<L>\t%s %s<R>%i", "Colonists",
                                      pd.workers + pd.scientists + pd.kids, "Available", "Workers", c.workersLeft,
                                      "Available", "Scientists", c.scientistsLeft));
                    break;
                case 2: {                                   // sub_469730
                    const char* trend = colonyMoraleForced(me) ? "(Steady)"
                                      : c.moraleTrend > 0 ? "Positive" : c.moraleTrend < 0 ? "Negative" : "Stable";
                    out.push_back(fmt("%s<R>%i\n<L>\t%s<R>%s", "Morale Level", c.moraleValue, "Trend", trend));
                    break;
                }
                case 3:
                    out.push_back(fmt("%s<R>%i/%i\n<L>\t%s<R>%i", "Power Use",
                                      std::max(0, c.powerGenerated - c.powerLeft), c.powerGenerated,
                                      "Unpowered Structures", c.unpoweredCount));
                    break;
                case 4:
                    out.push_back(fmt("%s<R>%i\n<L>\t%s<R>%i", "Food Stored", pd.foodStored, "Production", c.foodProduced));
                    break;
                case 5:
                    out.push_back(fmt("%s<R>%i\n<L>\t%s:%i<R>%s:%i", "Common Metals", pd.ore,
                                      "Mines", c.stat(ST_MINE), "Smelters", c.stat(ST_SMELT_GRP)));
                    break;
                case 6:
                    out.push_back(fmt("%s<R>%i\n<L>\t%s:%i<R>%s:%i", "Rare Metals", pd.rareOre,
                                      "Mines", c.stat(ST_RMINE), "Smelters", c.stat(ST_RSMELT_GRP)));
                    break;
                default: out.push_back(""); break;
            }
        } else if (m_mode == 2) {
            auto pct = [&](const char* name, int val, const char* none) {
                return val == 10000 ? std::string(none) : fmt("%s<R>%i%%", name, val);
            };
            auto oper = [&](const char* name, int on) {
                return on ? fmt("%s %s", name, "Operational") : fmt("%s %s %s", name, "Not", "Operational");
            };
            switch (v) {
                case 1:  out.push_back(pct("Residence Demand", c.crowding, "No Residences")); break;
                case 2:  out.push_back(fmt("%s<R>%i", "Net Food Production", c.foodProduced - c.foodEaten)); break;
                case 3:  out.push_back(pct("Disabled Bldg. Ratio", c.disabledPct, "No Active Structures")); break;
                case 4:  out.push_back(fmt("%s<R>%i%%", "Unoccupied Colonists", c.idlePct)); break;
                case 5:  out.push_back(fmt("%s<R>%i%%", "Scientists as Workers", c.phdPct)); break;
                case 6:  out.push_back(pct("Rec./Forum Demand", c.recreationLoad, "No Rec./Forums")); break;
                case 7:  out.push_back(pct("Rec. Facility Demand", c.recreationLoad, "No Recreation Facilities")); break;
                case 8:  out.push_back(pct("Med. Center Demand", c.medicalLoad, "No Med. Centers")); break;
                case 9:  out.push_back(oper("Nursery", c.hasNursery)); break;
                case 10: out.push_back(oper("University", c.hasUniversity)); break;
                case 11: out.push_back(oper("GORF", c.hasGorf)); break;
                case 12: out.push_back(fmt("%s<R>%i", "Disaster Warning\n\tSystems", c.disasterTechs)); break;
                case 13: out.push_back(fmt("%s<R>%i.%02i%%", "DIRT: Average Damage\n\tPrevention",
                                           c.dirtCoverage / 100, c.dirtCoverage % 100)); break;
                default: out.push_back(""); break;
            }
        } else {                                            // 7..12: prédios
            const UnitRecord& r = g_units[v];
            std::string s = unitTitle(r);
            if (r.action == 0x12 && !(r.flags & 0x40000)) s += "\n<C5>\tIdle";
            else if (r.action == ACT_BUILD) s += "\n<C0>\tUnder Construction";
            else if (structureOperational(r)) s += "\n<C0>\tActive";
            else s += "\n<C2>\tDisabled";
            if (r.type == 0x15 || r.type == 0x16) {         // vt[0x108] rendimento, vt[0x104] carga
                static const char* kYield[3] = {"High-Yield", "Medium-Yield", "Low-Yield"};
                const int y = (r.mineYield >= 0 && r.mineYield <= 2) ? r.mineYield : 0;
                s += fmt(", %s (%i)", kYield[y], mineLoadAmount(v));
            }
            out.push_back(s);
        }
    }
    return out;
}

void ResourcesPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    static const char* kTitle[13] = {
        "Resources Report", "Colonists Status (Alt+C)", "Morale Status (Alt+M)", "Power Status (Alt+P)",
        "Food Status (Alt+F)", "Common Metals Status (Alt+N)", "Rare Metals Status (Alt+R)",
        "Colonists Structures", "Morale Structures", "Power Structures", "Food Structures",
        "Common Metals Structures", "Rare Metals Structures"};
    const int me = TethysGame::localPlayer();
    const PlayerData& pd = g_playerData[me];
    const ColonyState& c = g_colony[me];
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, pane.t + 0x16},
                   kTitle[m_mode], kGreen, true, kMarkupLine);                       // sub_469250 → sub_459B60
    // Páginas (texto em L+0xD, conteúdo+0x14 — ou +0x28 nos metais —, R−0xE).
    PaneRect rc{pane.l + 0xD, pane.t + 0x14, pane.r - 0xE, pane.b};
    switch (m_mode) {
        case 1: {                                           // sub_4699D0
            const std::string s = fmt(
                "%s<R>%i\n<L>%s<R>%i\n\n<L>%s<R>%i\n<L>\n%s<R>%i\n<L>\t%s<R>%i/%i    \n<L>\t%s<R>%i    \n<L>\n"
                "%s<R>%i\n<L>\t%s<R>%i/%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>",
                "Total Population", pd.workers + pd.scientists + pd.kids,
                "Total Residence Capacity", c.stat(ST_RES_CAP),
                "Children", pd.kids,
                "Workers", pd.workers,
                "Assigned", std::min(pd.workers, c.workersRequired), c.workersRequired,
                "Available", c.workersLeft,
                "Scientists", pd.scientists,
                "Assigned as Scientists", pd.scientists - c.scientistsAsWorkers - c.researchAssigned - c.scientistsLeft,
                c.scientistsRequired - c.researchAssigned,
                "Assigned as Workers", c.scientistsAsWorkers,
                "Assigned to Research", c.researchAssigned,
                "Available", c.scientistsLeft);
            drawMarkupText(fb, fbW, fbH, rc, s, kGreen, false, kMarkupLine);
            break;
        }
        case 2: {                                           // sub_469B90
            static const char* kLevel[5] = {"Excellent", "Good", "Fair", "Poor", "Terrible"};
            const int lv = (pd.morale >= 0 && pd.morale <= 4) ? pd.morale : 4;
            std::string name = kLevel[lv];
            if (colonyMoraleForced(me)) name += std::string(" ") + "(Steady)";
            drawMarkupText(fb, fbW, fbH, rc, name + "<R>" + std::to_string(c.moraleValue), kGreen, false, kMarkupLine);
            drawProgressBar(fb, fbW, fbH, rc.l, rc.t + 0xF, rc.r, rc.t + 0xF + 6, c.moraleValue, 0x63, 0x3E0, 0x1E0);
            break;
        }
        case 3: {                                           // sub_469E40
            const int consumed = std::max(0, c.powerGenerated - c.powerLeft);
            const int more = std::max(0, c.powerRequired - c.powerLeft - consumed);
            const std::string s = fmt(
                "%s<R>%i\n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\n%s<R>%i\n<L>%s<R>%i\n"
                "<L>%s<R>%i\n<L>%s<R>%i\n\n<L>%s<R>%i\n<L>%s<R>%i",
                "Power Generation Facilities", c.stat(ST_POWER_GRP), "Active", c.stat(ST_POWER_GRP + 4),
                "Disabled", c.stat(ST_POWER_GRP + 8), "Idled", c.stat(ST_POWER_GRP + 12),
                "Power Generated", c.powerGenerated, "Power Consumed", consumed, "Unused Power", c.powerLeft,
                "Inactive Power Capacity", c.powerIdle, "Structures Disabled/Power", c.unpoweredCount,
                "Additional Power Needed", more);
            drawMarkupText(fb, fbW, fbH, rc, s, kGreen, false, kMarkupLine);
            break;
        }
        case 4: {                                           // sub_469FD0
            const std::string s = fmt(
                "%s<R>%i\n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\n%s<R>%i\n<L>%s<R>%i\n<L>%s<R>%i",
                "Agridomes", c.stat(ST_AGRI_GRP), "Active", c.stat(ST_AGRI_GRP + 4),
                "Disabled", c.stat(ST_AGRI_GRP + 8), "Idled", c.stat(ST_AGRI_GRP + 12),
                "Food Produced", c.foodProduced, "Food Consumed", c.foodEaten, "Food in Storage", pd.foodStored);
            drawMarkupText(fb, fbW, fbH, rc, s, kGreen, false, kMarkupLine);
            PaneRect r2 = rc;
            r2.t += 0x6E;
            drawMarkupText(fb, fbW, fbH, r2, c.foodNet < 0 ? fmt("<C1>%s<R>%i", "Taken from Storage", -c.foodNet)
                                                          : fmt("%s<R>%i", "Added to Storage", c.foodNet), kGreen, false, kMarkupLine);
            if (c.foodDeficit) {
                r2.t += 0xF;
                drawMarkupText(fb, fbW, fbH, r2, fmt("<C2>%s<R>%i", "Food Shortage", c.foodDeficit), kGreen, false, kMarkupLine);
            }
            break;
        }
        case 5: case 6: {                                   // sub_46A280 / sub_46A440
            const bool rare = m_mode == 6;
            const int mine = rare ? ST_RMINE : ST_MINE, smelt = rare ? ST_RSMELT_GRP : ST_SMELT_GRP;
            const int store = rare ? ST_RSTORE_GRP : ST_STORE_GRP;
            const std::string s = fmt(
                "%s<R>%i\n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\n"
                "%s<R>%i\n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\n"
                "%s<R>%i\n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\t%s<R>%i    \n<L>\n%s<R>%i\n<L>%s<R>%i",
                "Mines", c.stat(mine), "High-Yield", c.stat(mine + 4), "Medium-Yield", c.stat(mine + 8),
                "Low-Yield", c.stat(mine + 12),
                "Smelters", c.stat(smelt), "Active", c.stat(smelt + 4), "Disabled", c.stat(smelt + 8),
                "Idled", c.stat(smelt + 12),
                "Storage Tanks", c.stat(store), "Active", c.stat(store + 4), "Disabled", c.stat(store + 8),
                "Idled", c.stat(store + 12),
                "Metal in Storage", rare ? pd.rareOre : pd.ore, "Storage Capacity", rare ? c.rareCapacity : c.oreCapacity);
            rc.t = pane.t + 0x28;
            drawMarkupText(fb, fbW, fbH, rc, s, kGreen, false, kMarkupLine);
            break;
        }
        default: break;
    }
    if (hasList()) m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

void ResourcesPane::select(int item) {                      // sub_469630
    if (item < 0 || item >= static_cast<int>(m_ids.size())) return;
    if (m_mode == 0) { m_detailMode = m_ids[item]; m_chosen = true; m_list.sel = item; }
    else if (m_mode >= 7) { m_unit = m_ids[item]; m_chosen = true; m_list.sel = item; }
}

bool ResourcesPane::click(const PaneRect& pane, int x, int y, bool doubleClick, bool& dbl) {
    dbl = false;
    if (!m_open || !hasList()) return false;
    const int keep = m_list.sel;
    int item;
    if (!m_list.click(layout(pane), items(), x, y, item)) return false;
    if (m_mode == 2) { m_list.sel = keep; return true; }    // fatores de moral: sem escolha
    if (item >= 0) {
        select(item);
        dbl = doubleClick;                                  // vt[10] sub_4695D0
    }
    return true;
}

bool ResourcesPane::detailsEnabled() const {
    return m_open && (m_mode == 0 || m_mode >= 7) && m_chosen;
}

} // namespace op2
