#include "storage_pane.h"

#include <string>

#include "../gfx/art_blit.h"
#include "../gfx/artfile.h"
#include "../gfx/font.h"
#include "../game/building_sheet.h"
#include "../game/colony.h"
#include "../game/docking.h"
#include "../game/player.h"
#include "../game/structure.h"
#include "../game/structure_anim.h"
#include "../game/unit.h"
#include "../game/vehicle_anim.h"

namespace op2 {

static uint32_t rgb555(uint16_t c) {
    const uint32_t r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
static bool edenBuilt(const UnitRecord& r) {          // [+0x44] & 0x1000
    const int o = r.owner & 0x0F;
    return o < kMaxPlayers && g_playerData[o].isEden != 0;
}

int StoragePane::bays() const {
    const UnitRecord& b = g_units[m_bld];
    const StructSheetDesc* d = structSheet(b.type);
    const int n = d ? d->player[(b.owner >> 4) & 0x0F].storageBays : 0;
    return n > 6 ? 6 : n;
}

void StoragePane::update(const std::vector<int>& sel) {
    if (!m_open) return;
    if (sel.size() != 1 || sel[0] != m_bld || m_bld <= 0 || m_bld >= static_cast<int>(g_units.size()) ||
        g_units[m_bld].id == -1 || (g_units[m_bld].flags & UNIT_DEAD))
        m_open = false;
}

PaneRect StoragePane::bayRect(int winL, int winT, int i) const {   // sub_463170
    const int x = winL + (i < 3 ? 7 : 0x61);
    const int y = winT + 0x39 + 0x3A * (i % 3);
    return PaneRect{x, y, x + 0x2C, y + 0x2C};
}

// Nome do item da baia (sub_4637B0): +0x1F8 do tipo; Guard Post → a arma.
static std::string bayName(const UnitRecord& b, int i) {
    const int t = static_cast<int8_t>(b.bayType[i]);
    if (!t) return "Empty Bay";
    if (t == 0x17) return unitDisplayName(static_cast<int8_t>(b.bayCargo[i]));
    return unitDisplayName(t);
}

// Ícone da baia (sub_463170): kit de prédio = estado 4 da tabela (desc+0x260
// Eden / +0x288 Plymouth); módulo espacial 0x707; outros 0x4B4; vazia 0x706.
static int bayIcon(const UnitRecord& b, int i) {
    const int t = static_cast<int8_t>(b.bayType[i]);
    if (!t) return 0x706 - 0x65;
    if (t >= 0x15 && t <= 0x3A) return structureAnimIndex(t, 4, edenBuilt(b));
    if (t >= 0x5A && t <= 0x63) return 0x707 - 0x65;
    return 0x4B4 - 0x65;
}

// Quebra por palavras na largura (sub_416650), preservando as quebras do texto.
static std::string wrapText(const std::string& text, int width) {
    std::string out, line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        const std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && Font::getTextWidth(cand) > width) { out += line + "\n"; line = word; }
        else line = cand;
        word.clear();
    };
    for (char ch : text) {
        if (ch == ' ') flushWord();
        else if (ch == '\n') { flushWord(); out += line + "\n"; line.clear(); }
        else word += ch;
    }
    flushWord();
    return out + line;
}

void StoragePane::draw(uint32_t* fb, int fbW, int fbH, int winL, int winT, const PaneRect& pane,
                       int statusTop, int mx, int my) const {
    if (!m_open) return;
    const UnitRecord& b = g_units[m_bld];
    // Título (unk_578F08, verde).
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 1, pane.r - 0xD, pane.t + 20},
                   unitTitle(b), 0xFF00FF00u);
    const int n = bays();
    for (int i = 0; i < n; ++i) {
        const PaneRect r = bayRect(winL, winT, i);
        const int icon = bayIcon(b, i);
        if (g_art.ok() && icon >= 0 && icon < g_art.numAnimations())
            blitArtFrame(fb, fbW, fbH, icon, 0, r.l, r.t, -1, pane.l, pane.t, pane.r, pane.b);
        const bool over = mx >= r.l && mx < r.r && my >= r.t && my < r.b;
        if (over && bayTransferAvailable(m_bld, i))
            for (int x = r.l; x < r.r; ++x) { fb[r.t * fbW + x] = 0xFF00FF00u; fb[(r.b - 1) * fbW + x] = 0xFF00FF00u; }
        // Nome sob o ícone (x .. x+0x57, y = 0x64 + 0x3A*k) e "Alt+N" à direita.
        const int lx = winL + (i < 3 ? 7 : 0x61), ly = winT + 0x64 + 0x3A * (i % 3);
        std::string name = bayName(b, i);
        while (!name.empty() && Font::getTextWidth(name) > 0x55) name.pop_back();
        Font::drawTextCentered(fb, fbW, fbH, name, lx + 0x57 / 2, ly, 0xFF00FF00u, false);
        const std::string alt = "Alt+" + std::to_string(i + 1);
        Font::drawTextCentered(fb, fbW, fbH, alt, lx + (0x39 + 0x57) / 2, ly - 0x1E + 8, 0xFF00FF00u, false);
    }
    // Caixa da doca (0x33 acima do botão STATUS) e o texto à direita.
    const int by = statusTop - 0x33, bx = winL + 0xD;
    const int v = dockedVehicle(m_bld);
    std::string text;
    if (v) {
        const UnitRecord& u = g_units[v];
        const bool eden = (u.owner & 0x0F) < kMaxPlayers && g_playerData[u.owner & 0x0F].isEden;
        const int anim = vehicleAnimIndex(u.type, u.rotation, eden);
        // (sub_458EF0 — ícone do veículo com a carga — não portado: a animação parada.)
        if (g_art.ok() && anim >= 0)
            blitArtFrame(fb, fbW, fbH, anim, 0, bx + 0x16, by + 0x16, (u.owner & 0x0F) < kMaxPlayers
                         ? g_playerData[u.owner & 0x0F].colorNumber : 0, bx, by, bx + 0x2C, by + 0x2C);
        const std::string vname = unitTitle(u);
        const int cargo = static_cast<int16_t>(u.cargo);
        if (cargo) {
            const std::string cname = cargo == 0x17 ? unitDisplayName(u.kitWeapon) : unitDisplayName(cargo);
            text = cname + " carried in " + vname;
        } else {
            text = vname + " is empty";
        }
    } else {
        const uint32_t fillC = rgb555(edenBuilt(b) ? 0x6DC3 : 0x35AD);
        for (int y = by; y < by + 0x2C; ++y)
            for (int x = bx; x < bx + 0x2C; ++x)
                if (x >= 0 && x < fbW && y >= 0 && y < fbH) fb[y * fbW + x] = fillC;
        const int emptyAnim = 0x6FE - 0x65;
        if (g_art.ok() && emptyAnim < g_art.numAnimations())
            blitArtFrame(fb, fbW, fbH, emptyAnim, 0, bx + 0x17, by + 0x17, -1, bx, by, bx + 0x2C, by + 0x2C);
        // [jogador+0x17C] (GORF): com recicladora as transferências viram minério.
        bool gorf = false;
        for (int i = 1; i < static_cast<int>(g_units.size()) && !gorf; ++i) {
            const UnitRecord& g = g_units[i];
            gorf = g.id != -1 && g.type == 0x39 && (g.owner & 0x0F) == (b.owner & 0x0F) && structureOperational(g);
        }
        text = gorf ? "Dock is empty.\nTransfers are recycled" : "Dock is empty.\nTransfers go to trash heap";
    }
    drawMarkupText(fb, fbW, fbH, PaneRect{winL + 0x47, by, pane.r - 4, by + 0x2C},
                   wrapText(text, pane.r - 4 - (winL + 0x47)), 0xFF00FF00u);
}

void StoragePane::pressBay(int bay) {
    if (!m_open || bay < 0 || bay >= bays()) return;
    if (bayTransferAvailable(m_bld, bay)) sendBayTransfer(m_bld, bay);
}

bool StoragePane::click(int winL, int winT, int x, int y) {
    if (!m_open) return false;
    for (int i = 0; i < bays(); ++i) {
        const PaneRect r = bayRect(winL, winT, i);
        if (x >= r.l && x < r.r && y >= r.t && y < r.b) { pressBay(i); return true; }
    }
    return false;
}

std::string StoragePane::tipAt(int winL, int winT, int x, int y) const {
    if (!m_open) return {};
    const UnitRecord& b = g_units[m_bld];
    for (int i = 0; i < bays(); ++i) {
        const PaneRect r = bayRect(winL, winT, i);
        if (!(x >= r.l && x < r.r && y >= r.t && y < r.b)) continue;
        // Dica: nome do item; baia vazia com veículo na doca → "Get cargo from docked vehicle".
        if (!b.bayType[i]) return dockedVehicle(m_bld) ? "Get cargo from docked vehicle" : "";
        return unitDisplayName(static_cast<int8_t>(b.bayType[i]));
    }
    return {};
}

} // namespace op2
