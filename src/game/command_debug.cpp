#include "command_debug.h"

#include "building_sheet.h"
#include "messages.h"
#include "structure.h"
#include "tethys_game.h"
#include "unit.h"

#include <cstdio>

namespace op2 {

static bool s_on = false;
static std::string s_last;

void setCommandDebug(bool on) { s_on = on; }
bool commandDebug() { return s_on; }
const std::string& commandLastRefusal() { return s_last; }
void commandClearLastRefusal() { s_last.clear(); }

const char* structureDownReason(const UnitRecord& r) {
    // Na ordem em que um jogador consegue resolver: o estado da obra, a
    // ordem dele, o dano, e por fim o que a colonia nao entrega.
    if (r.action == ACT_BUILD) return "still under construction";
    if (r.action == ACT_DISMANTLE) return "being dismantled";
    if (r.action == 0x12) return "idled";
    if ((r.flags & UNIT_BUILDING) && (structureMaxHP(r) * 3 >> 2) <= r.damage) return "too damaged";
    if (r.flags & UNIT_DISABLED_TIMERS) return "disabled (EMP)";
    if (!(r.flags & UNIT_POWERED)) return r.netConnected ? "not enough power" : "no tube link to CC";
    if (!(r.flags & UNIT_WORKERS)) return "not enough workers";
    if (!(r.flags & UNIT_SCIENTISTS)) return "not enough scientists";
    return "";
}

void commandRefused(int owner, int unitId, const char* why) {
    if (!s_on || !why || !*why) return;
    if ((owner & 0x0F) != TethysGame::localPlayer()) return;
    int x = -1, y = -1;
    const char* nome = "";
    if (unitId > 0 && unitId < static_cast<int>(g_units.size()) && g_units[unitId].id != -1) {
        const UnitRecord& u = g_units[unitId];
        x = u.pixelX + 16; y = u.pixelY + 16;
        nome = unitDisplayName(u.type);
    }
    char buf[96];
    if (nome && *nome) std::snprintf(buf, sizeof(buf), "%s: %s", nome, why);
    else std::snprintf(buf, sizeof(buf), "Refused: %s", why);
    s_last = buf;
    std::printf("[CmdDebug] tick %u: %s\n", static_cast<unsigned>(TethysGame::tick()), buf);
    std::fflush(stdout);
    postMessageQuiet(x, y, buf);
}

} // namespace op2
