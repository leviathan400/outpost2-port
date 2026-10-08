#include "info_pane.h"

#include <cstdio>
#include <vector>

#include "../gfx/font.h"
#include "../gfx/art_blit.h"
#include "../game/unit.h"
#include "../game/player.h"
#include "../game/colony.h"
#include "../game/building_sheet.h"
#include "../game/structure.h"
#include "../game/structure_anim.h"

namespace op2 {

// Cores das tags <Cn> do texto marcado (switch em sub_416650, COLORREF
// 0x00BBGGRR convertido p/ ARGB): C0 verde, C1 amarelo, C2 vermelho, C3 preto,
// C4 cinza, C5 azul-claro.
static const uint32_t kMarkupColor[6] = {
    0xFF00FF00u, 0xFFDDFF22u, 0xFFFF0000u, 0xFF000000u, 0xFF636363u, 0xFF6464FFu,
};
static const uint32_t kTextGreen = 0xFF00FF00u;   // 0xFF00 passado aos desenhos de texto

// RGB555 → ARGB (o painel original desenha barras em 16 bits).
static uint32_t rgb555(uint16_t c) {
    uint32_t r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

int markupLineHeight() { return Font::getTextHeight() + 3; }

int drawMarkupText(uint32_t* fb, int fbW, int fbH, const PaneRect& rc,
                   const std::string& text, uint32_t defaultColor) {
    struct Seg { std::string s; uint32_t color; int align; };   // 0 esq, 1 centro, 2 dir
    const int lh = markupLineHeight();
    int y = rc.t;
    size_t pos = 0;
    uint32_t color = defaultColor;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        std::vector<Seg> segs;
        int align = 0;
        std::string cur;
        auto flush = [&]() { if (!cur.empty()) { segs.push_back({cur, color, align}); cur.clear(); } };
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '<') {
                size_t e = line.find('>', i);
                if (e != std::string::npos && e > i + 1) {
                    const char t = line[i + 1];
                    if (t == 'C' && e == i + 3 && line[i + 2] >= '0' && line[i + 2] <= '5') {
                        flush(); color = kMarkupColor[line[i + 2] - '0']; i = e; continue;
                    }
                    if (e == i + 2 && (t == 'L' || t == 'R' || t == 'X')) {
                        flush(); align = (t == 'L') ? 0 : (t == 'X') ? 1 : 2; i = e; continue;
                    }
                }
            }
            cur += line[i];
        }
        flush();
        int xl = rc.l, wr = 0, wc = 0;
        for (const auto& s : segs) {
            if (s.align == 2) wr += Font::getTextWidth(s.s);
            if (s.align == 1) wc += Font::getTextWidth(s.s);
        }
        int xr = rc.r - wr, xc = (rc.l + rc.r - wc) / 2;
        for (const auto& s : segs) {
            int& x = (s.align == 0) ? xl : (s.align == 1) ? xc : xr;
            Font::drawText(fb, fbW, fbH, s.s, x, y, s.color, false);
            x += Font::getTextWidth(s.s);
        }
        y += lh;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return y - rc.t;
}

static void fillRect(uint32_t* fb, int fbW, int fbH, int x, int y, int w, int h, uint32_t c) {
    for (int yy = y; yy < y + h; ++yy) {
        if (yy < 0 || yy >= fbH) continue;
        for (int xx = x; xx < x + w; ++xx)
            if (xx >= 0 && xx < fbW) fb[yy * fbW + xx] = c;
    }
}
static void frameRect(uint32_t* fb, int fbW, int fbH, int x, int y, int w, int h, uint32_t c) {
    fillRect(fb, fbW, fbH, x, y, w, 1, c);
    fillRect(fb, fbW, fbH, x, y + h - 1, w, 1, c);
    fillRect(fb, fbW, fbH, x, y, 1, h, c);
    fillRect(fb, fbW, fbH, x + w - 1, y, 1, h, c);
}

// Valor de blindagem do bloco (+0x08, já dword_4E1510[classe]) → nome
// (Format[0xEA..0xF1]).
static const char* armorName(int v, bool isBuilding) {
    if (isBuilding) {
        if (v >= 0x100) return "No Armor";
        if (v >= 0xB4) return "Very Light Armor";
        if (v >= 0x82) return "Light Armor";
        if (v >= 0x5A) return "Medium-Light Armor";
        if (v >= 0x3C) return "Medium Armor";
        return "Heavy Armor";
    }
    if (v >= 0x100) return "No Armor";
    if (v >= 0xB4) return "Light Armor";
    if (v >= 0x82) return "Medium Armor";
    if (v >= 0x5A) return "Heavy Armor";
    if (v >= 0x3C) return "Very Heavy Armor";
    return "Adamantium Armor";
}

// ---- Seção de Status (sub_45A8F0) -------------------------------------------
static int drawStatusSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc,
                             const UnitRecord& r, int localPlayer) {
    const StructSheetDesc* d = structSheet(r.type);
    const int p = r.owner & 0x0F;
    const StructPlayerStats* s = d ? &d->player[p] : nullptr;
    const int powReq = s ? s->power : 0;                       // bloco+0x24
    int powShown = 0, powColor = 0, statusColor = 0;
    bool disabled = false;
    std::string status;
    if (r.action == 0x12 && !(r.flags & 0x40000)) { status = "Idle"; statusColor = 5; }
    else if (r.action == ACT_BUILD) status = "Under Construction";
    else if (structureOperational(r)) { status = "Active"; powShown = powReq; }
    else {
        if (r.flags & UNIT_POWERED) powShown = powReq; else powColor = 2;
        status = "Disabled"; disabled = true; statusColor = 2;
    }
    if (!disabled) powColor = 0;
    char power[64];
    if (powReq) std::snprintf(power, sizeof(power), "<L><C0>%s:<R><C%d>%d/%d", "Power", powColor, powShown, powReq);
    else std::snprintf(power, sizeof(power), "<L><C0>%s:<R>%s", "Power", "None");
    // Trabalhadores (bloco+0x28).
    const int wReq = s ? s->workers : 0;
    int wShown = wReq, wColor = 0;
    if (!(r.flags & UNIT_WORKERS)) { wShown = 0; wColor = 2; }
    if (!disabled) wColor = 0;
    char workers[64];
    if (wReq) std::snprintf(workers, sizeof(workers), "<L><C0>%s:<R><C%d>%d/%d", "Workers", wColor, wShown, wReq);
    else std::snprintf(workers, sizeof(workers), "<L><C0>%s:<R>%s", "Workers", "None");
    // Cientistas (vt+0xE0). (Labs exibem o máximo do tópico de pesquisa — com a pesquisa.)
    const int sReq = (s ? s->scientists : 0) + ((r.type == 0x21 || r.type == 0x31 || r.type == 0x32) ? r.extraScientists : 0);
    int sShown = sReq, sColor = 0;
    if (!(r.flags & UNIT_SCIENTISTS)) { sShown = 0; sColor = 2; }
    if (!disabled) sColor = 0;
    char sci[64];
    if (sReq) std::snprintf(sci, sizeof(sci), "<L><C0>%s:<R><C%d>%d/%d", "Scientists", sColor, sShown, sReq);
    else std::snprintf(sci, sizeof(sci), "<L><C0>%s:<R>%s", "Scientists", "None");
    // Motivo do "Disabled" ("-" + motivo).
    if (disabled) {
        const ColonyState& c = g_colony[localPlayer];
        const char* why = "Unknown";
        if (r.flags & 0x40000) why = "Infected";
        else if (r.flags & 0x80000) why = "EMP";      // off_4E8CC4 -> "EMP" (0x4E15C4)
        else if ((wReq || sReq) && !r.netConnected && r.type != mapCommandCenter) why = "Cmd Center";
        else if (s && ((s->hitPoints * 3) >> 2) <= r.damage) why = "Damage";
        else if (powShown != powReq) why = "Power";
        else if (!(r.flags & UNIT_WORKERS) && c.workersLeft < wReq) why = "Workers";
        else if (!(r.flags & UNIT_SCIENTISTS) && c.scientistsLeft < sReq) why = "Scientists";
        else if (r.flags & UNIT_SOLAR_IDLE) why = "No Satellite";
        status += "-";
        status += why;
    }
    char st[96];
    std::snprintf(st, sizeof(st), "<C0>%s:<R><C%d>%s", "Status", statusColor, status.c_str());
    std::string all = std::string(st) + "\n" + power + "\n" + workers + "\n" + sci;
    drawMarkupText(fb, fbW, fbH, rc, all, kTextGreen);
    return 4 * markupLineHeight();
}

// ---- Seção de Produção (sub_45AEB0) -----------------------------------------
static int drawProductionSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc,
                                 const UnitRecord& r) {
    const char* what = nullptr;
    int level = 0;
    switch (r.type) {
        case 0x1C: case 0x1D: case 0x24: case 0x2C: case 0x3A:     // CC + geradores
            what = "Power"; level = structurePowerOutput(r); break;
        case 0x27: what = "Food"; level = structureFoodOutput(r); break;   // Agridome
        default: return 0;
    }
    if (r.action == 0x12 || !structureOperational(r)) level = 0;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s:<R>%s\n<L>%s:<R>%d", "Production", what, "Production Level", level);
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
}

void drawUnitInfoPane(uint32_t* fb, int fbW, int fbH, const PaneRect& pane,
                      int unitId, int numSelected, int localPlayer) {
    if (numSelected <= 0 || unitId <= 0 || unitId >= static_cast<int>(g_units.size())) return;
    const UnitRecord& r = g_units[unitId];
    if (r.id == -1) return;
    const bool isBld = (r.flags & UNIT_BUILDING) != 0;
    const int W = pane.r - pane.l;
    const int L = pane.l + 13, R = pane.l + W - 13;          // sub_459480: 13 .. W-13
    const int lh = markupLineHeight();
    // Título (unk_578F08): nome "# jogador-série" ou "%i Vehicles".
    const int titleTop = pane.t + 1;
    const int titleBottom = titleTop + lh + 2;
    char title[96];
    if (numSelected > 1) {
        std::snprintf(title, sizeof(title), "%d %s", numSelected, "Vehicles");
    } else {
        std::snprintf(title, sizeof(title), "%s", unitTitle(r).c_str());   // vt+0x40
    }
    PaneRect trc{L, titleTop, R, titleBottom};
    drawMarkupText(fb, fbW, fbH, trc, title, 0xFFFFFFFFu);
    // Separador (anim 0x9B → real 0x36) sob o título.
    blitArtFrame(fb, fbW, fbH, 0x9B - 0x65, 0, L - 10, titleBottom - 3, -1,
                 pane.l, pane.t, pane.r, pane.b);
    if (numSelected > 1) return;
    setArtShadows(true);
    // Ícone 44x44: prédio = animação do ESTADO 4 da tabela (vt+0x90(4)).
    const int iconTop = titleBottom + 4, iconL = L - 3;
    const int pc = g_playerData[localPlayer < kMaxPlayers ? localPlayer : 0].colorNumber;
    if (isBld) {
        const bool tableA = g_playerData[r.owner & 0x0F].isEden != 0;
        const int icon = structureAnimIndex(r.type, 4, tableA);
        if (icon >= 0) blitArtFrame(fb, fbW, fbH, icon, 0, iconL, iconTop, pc,
                                    pane.l, pane.t, pane.r, pane.b);
    }
    // (Ícone de veículo — sub_458EF0, com a carga/arma — ainda não portado.)
    // Blindagem + HP à direita do ícone.
    int hp = 0, armorClass = 256;
    if (isBld) {
        if (const StructSheetDesc* d = structSheet(r.type)) {
            hp = d->player[r.owner & 0x0F].hitPoints; armorClass = d->player[r.owner & 0x0F].armor;
        }
    } else if (const VehPlayerStats* vs = vehicleSheet(r.type, r.owner & 0x0F)) {
        hp = vs->hitPoints; armorClass = vs->armor;
    }
    const int tx = iconL + 44 + 9;
    PaneRect arc{tx, iconTop, R, iconTop + lh};
    drawMarkupText(fb, fbW, fbH, arc, armorName(armorClass, isBld), kTextGreen);
    char hpTxt[32];
    std::snprintf(hpTxt, sizeof(hpTxt), "%d/%d", hp - r.damage, hp);
    PaneRect hrc{tx, iconTop + lh, R, iconTop + 2 * lh};
    drawMarkupText(fb, fbW, fbH, hrc, hpTxt, kTextGreen);
    // Barra de HP (sub_45C260): moldura cinza 0x3DEF, parte cheia proporcional
    // ao HP restante em verde (0x3E0), amarelo (0x7FE0, dano > 40%) ou
    // vermelho (0x7C00, dano >= 75%); o resto em cinza.
    {
        const int by = iconTop + 2 * lh, bh = 6;
        uint16_t col = 0x3E0;
        if (((hp * 3) >> 2) <= r.damage) col = 0x7C00;
        else if (hp * 6 / 10 < r.damage) col = 0x7FE0;
        frameRect(fb, fbW, fbH, tx, by, R - tx, bh, rgb555(0x3DEF));
        const int inner = R - tx - 2;
        const int cur = hp > 0 ? (hp - r.damage) * inner / hp : 0;
        fillRect(fb, fbW, fbH, tx + 1, by + 1, inner, bh - 2, rgb555(0x3DEF));
        if (cur > 0) fillRect(fb, fbW, fbH, tx + 1, by + 1, cur, bh - 2, rgb555(col));
    }
    if (!isBld) return;
    // Seções (sub_45A510): retângulo 13..W-14, a partir de topo + título + 0x3C.
    PaneRect sec{pane.l + 13, pane.t + lh + 0x3C, pane.l + W - 14, pane.b - 1};
    sec.t += 2 * lh;                                   // sub_45A690: melhorias (2 linhas)
    sec.t += drawStatusSection(fb, fbW, fbH, sec, r, localPlayer);
    sec.t += drawProductionSection(fb, fbW, fbH, sec, r);
}

bool infoPaneButton(const PaneRect& pane, int unitId, int localPlayer, PaneButton& out) {
    if (unitId <= 0 || unitId >= static_cast<int>(g_units.size())) return false;
    const UnitRecord& r = g_units[unitId];
    if (r.id == -1 || !(r.flags & UNIT_BUILDING) || (r.owner & 0x0F) != localPlayer) return false;
    out.rc = PaneRect{pane.l + 12, pane.b - 24, pane.r - 12, pane.b - 8};
    if (r.action == 0x12 && !(r.flags & 0x40000)) { out.label = "ACTIVATE"; out.command = 0x13; return true; }
    if (structureCanIdle(r)) { out.label = "IDLE"; out.command = 0x12; return true; }
    return false;
}

} // namespace op2
