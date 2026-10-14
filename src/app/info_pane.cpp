#include "info_pane.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "../gfx/font.h"
#include "../gfx/ui_font.h"
#include "../gfx/art_blit.h"
#include "../game/unit.h"
#include "../game/player.h"
#include "../game/colony.h"
#include "../game/building_sheet.h"
#include "../game/structure.h"
#include "../game/structure_anim.h"
#include "../game/mining.h"
#include "../game/research.h"
#include "../game/vehicle_anim.h"
#include "../game/weapon.h"

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

int markupLineHeight() { return UiFont::lineHeight(); }   // sub_416A00: tmHeight

int drawMarkupText(uint32_t* fb, int fbW, int fbH, const PaneRect& rc,
                   const std::string& text, uint32_t defaultColor, bool bold) {
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
        int xl = rc.l, wl = 0, wr = 0, wc = 0;
        for (const auto& s : segs) {
            if (s.align == 0) wl += UiFont::width(s.s, bold);
            if (s.align == 2) wr += UiFont::width(s.s, bold);
            if (s.align == 1) wc += UiFont::width(s.s, bold);
        }
        // sub_416650 quebra a linha quando não cabe: o trecho à direita que
        // colidiria com o da esquerda desce p/ a linha seguinte.
        if (wl && wr && rc.l + wl + UiFont::width(" ", bold) > rc.r - wr) {
            for (const auto& s : segs)
                if (s.align == 0) { UiFont::draw(fb, fbW, fbH, s.s, xl, y, s.color, bold); xl += UiFont::width(s.s, bold); }
            y += lh;
            for (auto& s : segs) if (s.align == 0) s.s.clear();
        }
        int xr = rc.r - wr, xc = (rc.l + rc.r - wc) / 2;
        for (const auto& s : segs) {
            int& x = (s.align == 0) ? xl : (s.align == 1) ? xc : xr;
            UiFont::draw(fb, fbW, fbH, s.s, x, y, s.color, bold);
            x += UiFont::width(s.s, bold);
        }
        y += lh;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return y - rc.t;
}

// Quebra por palavras na largura do retângulo (sub_415EC0 / sub_416650).
static std::string wrapForPane(const std::string& text, const PaneRect& rc) {
    const int width = rc.r - rc.l;
    std::string out, line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        const std::string cand = line.empty() ? word : line + " " + word;
        if (!line.empty() && UiFont::width(cand) > width) { out += line + "\n"; line = word; }
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

// Texto de melhoria (+0x34) da tecnologia de índice `idx` (0 = nenhuma).
static std::string improveText(int idx) {
    if (idx <= 0) return "";
    const Tech* t = techAt(idx);
    return t ? t->improveDesc : "";
}
static int32_t blockDword(int type, int player, int off) {
    const uint8_t* b = unitStatBlock(type, player);
    int32_t v = 0;
    if (b) std::memcpy(&v, b + off, 4);
    return v;
}

// ---- Melhorias (sub_45A690): +0x3C/+0x40 do bloco (dono das estatísticas) ----
static int drawUpgradesSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r) {
    const int lh = markupLineHeight();
    const int p = (r.owner >> 4) & 0x0F;
    const std::string a = improveText(blockDword(r.type, p, 0x3C));
    const std::string b = improveText(blockDword(r.type, p, 0x40));
    if (!a.empty()) drawMarkupText(fb, fbW, fbH, rc, a, kTextGreen);
    if (!b.empty() && b != a) {
        PaneRect r2 = rc; r2.t += lh - 2;
        drawMarkupText(fb, fbW, fbH, r2, b, kTextGreen);
    }
    return 2 * lh;
}

// ---- Recurso do armazém (sub_45BD50) -----------------------------------------
static int drawResourceSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r) {
    if (r.type != 0x19 && r.type != 0x1A) return 0;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s:<R>%s", "Resource", r.type == 0x19 ? "Common Metals" : "Rare Metals");
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
}

// ---- Demanda da colônia (sub_45B540) — registro do jogador LOCAL --------------
static int drawDemandSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r, int me) {
    const ColonyState& c = g_colony[me];
    char buf[160];
    auto demand = [&](int v) {
        if (v != 10000) std::snprintf(buf, sizeof(buf), "%s:<R>%i%%", "Colony Demand", v);
        else std::snprintf(buf, sizeof(buf), "%s:<R>%s", "Colony Demand", "---");
    };
    switch (r.type) {
        case 0x1B: case 0x25: demand(c.recreationLoad); break;             // +0x54
        case 0x1E: case 0x34: case 0x35: demand(c.crowding); break;        // +0x48
        case 0x22: demand(c.medicalLoad); break;                           // +0x5C
        case 0x26: std::snprintf(buf, sizeof(buf), "%s:<R>%i", "Trainable Workers", c.workersLeft); break;   // +0xA4
        case 0x28: {                                                       // DIRT
            int local = 0;
            if (r.netComp >= 0 && r.netComp < static_cast<int>(g_colony[r.owner & 0x0F].components.size())) {
                const auto& k = g_colony[r.owner & 0x0F].components[r.netComp];
                const StructSheetDesc* d = structSheet(0x28);
                const int cap = d ? d->player[me].productionCapacity : 0;
                const int per = k.dirts ? (k.structures - k.dirts) / k.dirts : 0;   // sub_42FEE0
                local = cap ? per * 100 / cap : 0;
            }
            const int prot = colonyDirtProtection(r);                         // sub_42FF00
            std::snprintf(buf, sizeof(buf), "%s:<R>%i%%\n<L>%s:<R>%i.%02i%%", "Local Demand", local,
                          "Damage Prevention", prot / 100, prot % 100);
            break;
        }
        default: return 0;
    }
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
}

// ---- Rendimento da mina (sub_45B350): vt[0x104] -------------------------------
static int drawYieldSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, int id) {
    const UnitRecord& r = g_units[id];
    if (r.type != 0x15 && r.type != 0x16 && r.type != 0x2A) return 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s:<R>%i", "Yield", mineLoadAmount(id));   // ("%i%" no original)
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
}

// ---- Arma do Guard Post (sub_45BBD0): melhorias do bloco do jogador LOCAL -----
static int drawWeaponSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r, int me) {
    if (r.type != 0x17) return 0;
    const int w = static_cast<int16_t>(r.cargo);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s:<R>%s\n<L>%s\n%s", "Weapon", unitDisplayName(w),
                  improveText(blockDword(w, me, 0x3C)).c_str(), improveText(blockDword(w, me, 0x40)).c_str());
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
}

// ---- Observatório (sub_45B440): estatística +0x10C do jogador local -----------
static int drawObservatorySection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r, int me) {
    if (r.type != 0x2B) return 0;
    const char* t = g_colony[me].stat(ST_OBSERVATORY)
        ? "Observatory Active: Targeting data available."
        : "Observatory not Active: No targeting data available.";
    return drawMarkupText(fb, fbW, fbH, rc, wrapForPane(t, rc), kTextGreen);
}

// ---- Estoque (sub_45B070) ------------------------------------------------------
static int drawStorageSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r, int me) {
    const PlayerData& pd = g_playerData[me];
    const ColonyState& c = g_colony[me];
    const char* unit = "Units";
    int cap = 0, n = 0;
    switch (r.type) {
        case 0x19: case 0x36: cap = c.oreCapacity; n = pd.ore; break;
        case 0x1A: case 0x38: cap = c.rareCapacity; n = pd.rareOre; break;
        case 0x27: n = pd.foodStored; break;
        case 0x29: unit = "Vehicles"; n = 0; break;              // vt[63] da garagem (não portada)
        case 0x2F: {                                             // kits nas baias
            unit = "Kits";
            const StructSheetDesc* d = structSheet(r.type);
            const int bays = d ? d->player[(r.owner >> 4) & 0x0F].storageBays : 0;
            for (int i = 0; i < bays && i < 6; ++i) if (r.bayType[i]) ++n;
            break;
        }
        default: return 0;
    }
    char buf[128];
    if (!cap) std::snprintf(buf, sizeof(buf), "%s:<R>%i %s", "In Storage", n, unit);
    else std::snprintf(buf, sizeof(buf), "%s:<R>%i/%i %s", "In Storage", n, cap, unit);
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
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

// ---- Plataforma do Spaceport (sub_45BE70) ------------------------------------
// "Launch Pad:<R><veículo> (Loaded)/(empty)", só o míssil, ou "(empty)".
static int drawLaunchPadSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r) {
    if (r.type != 0x37) return 0;
    char buf[128];
    const int pad = r.padCargo;
    if (pad == 0x6B) std::snprintf(buf, sizeof(buf), "%s:<R>%s", "Launch Pad", unitDisplayName(pad));
    else if (pad) std::snprintf(buf, sizeof(buf), "%s:<R>%s %s", "Launch Pad", unitDisplayName(pad),
                                r.padPayload ? "(Loaded)" : "(empty)");
    else std::snprintf(buf, sizeof(buf), "%s:<R>%s", "Launch Pad", "(empty)");
    return drawMarkupText(fb, fbW, fbH, rc, buf, kTextGreen);
}

// Barra de progresso (sub_45C260): moldura 0x3DEF; feito em c1, o resto em c2.
static void progressBar(uint32_t* fb, int fbW, int fbH, int l, int t, int r, int b, int cur, int max,
                        uint16_t c1, uint16_t c2) {
    frameRect(fb, fbW, fbH, l, t, r - l, b - t, rgb555(0x3DEF));
    if (max <= 0) return;
    if (cur > max) cur = max;
    const int il = l + 1, ir = r - 2, w = ir - il + 1;
    const int fill = cur * w / max;
    if (fill > 0) fillRect(fb, fbW, fbH, il, t + 1, fill, b - t - 2, rgb555(c1));
    fillRect(fb, fbW, fbH, il + (fill > 0 ? fill : 0), t + 1, w - (fill > 0 ? fill : 0), b - t - 2, rgb555(c2));
}

void drawVehicleIcon(uint32_t* fb, int fbW, int fbH, bool eden, int type, int weapon,
                     int l, int t, int r, int b, const PaneRect& clip, int colorPlayer) {   // sub_458EF0
    fillRect(fb, fbW, fbH, l, t, r - l, b - t, rgb555(eden ? 0x6DC3 : 0x35AD));
    int dx = 0x14, dy = 0x14;
    bool tank = false;
    switch (type) {
        case 1: dx = 0x16; dy = 0x15; break;
        case 2: dx = 0x15; dy = 0x17; break;
        case 5: case 6: dx = (type == 6 && !eden) ? 0x12 : 0x16; dy = 0x16; tank = true; break;
        case 7: dx = 0x17; dy = 0x1A; tank = true; break;
        case 9: dx = 0x15; dy = eden ? 0x15 : 0x12; break;
        case 10: dx = 0x15; dy = 0x15; break;
        case 11: dx = 0x16; dy = 0x14; break;
        case 12: dx = 0x15; dy = 0x14; break;
        case 13: dx = 0x16; dy = 0x15; break;
        default: break;
    }
    const int body = vehicleAnimIndex(type, 0, eden);           // desc+0x24C / +0x34C
    if (body < 0) return;
    const int pc = g_playerData[colorPlayer < kMaxPlayers ? colorPlayer : 0].colorNumber;
    setArtShadows(false);                                       // dword_575630 = 0
    blitArtFrame(fb, fbW, fbH, body, 0, l + dx, t + dy, pc, clip.l, clip.t, clip.r, clip.b);
    if (tank) {
        const int tr = weaponTurretIconRaw(weapon, type, eden);
        if (tr) {
            const int8_t* o = artFrameOffsets(body + 0x65, 0);
            blitArtFrame(fb, fbW, fbH, tr - 0x65, 0, l + dx + o[0], t + dy + o[1], pc, clip.l, clip.t, clip.r, clip.b);
        }
    }
    setArtShadows(true);
}

// ---- Progresso: pesquisa / treino / produção (sub_45B850) -----------------------
static int drawProgressSection(uint32_t* fb, int fbW, int fbH, const PaneRect& rc, const UnitRecord& r,
                               int me, const PaneRect& clip) {
    char buf[160];
    int icon = 0, cur = 0, max = 0;
    bool vehicleIcon = false;
    const bool eden = g_playerData[r.owner & 0x0F].isEden != 0;      // [+0x44] & 0x1000
    switch (r.type) {
        case 0x21: case 0x31: case 0x32: {                           // labs
            if (r.action != 0x16 || !r.labTopic) return 0;
            const Tech* t = techAt(r.labTopic);
            if (!t) return 0;
            static const int kCat[12] = {0x4BB, 0x4B9, 0x4B1, 0x4B7, 0x4B6, 0x4BA, 0x4B5, 0x4B4, 0x4B8,
                                         0x4B3, 0x4B2, 0x707};         // sub_4320D0
            icon = (t->category >= 1 && t->category <= 12) ? kCat[t->category - 1] : 0;
            max = eden ? t->edenCost : t->plymouthCost;
            cur = max - (r.field3C >> 8);
            std::snprintf(buf, sizeof(buf), "%s\n%i/%i", t->name.c_str(), cur, max);
            break;
        }
        case 0x26: {                                                 // University
            if (r.action != 0x17) return 0;
            max = r.trainTime;
            cur = max - r.field3C;
            std::snprintf(buf, sizeof(buf), "%s %i %s:\n%i/%i", "Training", r.extraWorkers,
                          r.extraWorkers > 1 ? "Scientists" : "Scientist", cur, max);
            icon = 0x4BB;
            break;
        }
        case 0x2D: case 0x2E: case 0x2F: case 0x30: case 0x37: {     // fábricas
            if (r.action != 9) return 0;
            const int T = r.produceTotal;
            if (T <= 0) return 0;
            const int total = ((T + 20) / 40) * 25;
            std::snprintf(buf, sizeof(buf), "%s\n%i/%i", unitDisplayName(r.produceType),
                          total - r.field3C * total / T, total);
            cur = T - r.field3C; max = T;
            const int pt = r.produceType;
            switch (r.type) {                                        // vt[0x114]
                case 0x2D: icon = pt ? 0x795 : 0; break;
                case 0x2E: icon = pt ? 0x4B8 : 0; break;
                case 0x2F: icon = (pt >= 0x15 && pt <= 0x3A) ? structureAnimIndex(pt, 4, eden) + 0x65 : 0; break;
                case 0x37: icon = pt ? ((pt >= 0x5A && pt <= 0x63) ? 0x707 : 0x4B4) : 0; break;
                default: icon = pt ? 0x4B7 : 0; vehicleIcon = true; break;
            }
            break;
        }
        default: return 0;
    }
    blitArtFrame(fb, fbW, fbH, 0x9B - 0x65, 0, rc.l - 10, rc.t - 7, -1, clip.l, clip.t, clip.r, clip.b);
    const int pc = g_playerData[me].colorNumber;
    if (vehicleIcon && icon)
        drawVehicleIcon(fb, fbW, fbH, g_playerData[me].isEden != 0, r.produceType, r.kitWeapon,
                        rc.l - 3, rc.t, rc.l - 3 + 0x2D, rc.t + 0x2D, clip, me);
    else if (icon)
        blitArtFrame(fb, fbW, fbH, icon - 0x65, 0, rc.l - 3, rc.t, pc, clip.l, clip.t, clip.r, clip.b);
    PaneRect trc{rc.l + 0x33, rc.t, rc.r, rc.b};
    const int h = drawMarkupText(fb, fbW, fbH, trc, wrapForPane(buf, trc), kTextGreen);
    progressBar(fb, fbW, fbH, rc.l + 0x33, rc.t + h, rc.r, rc.t + h + 6, cur, max, 0x3E0, 0x1E0);
    return 0x2D;
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
    drawMarkupText(fb, fbW, fbH, trc, title, kTextGreen, true);   // fonte do título unk_578F08: verde
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
    else {
        const bool eden = g_playerData[r.owner & 0x0F].isEden != 0;   // [+0x44] & 0x1000
        drawVehicleIcon(fb, fbW, fbH, eden, r.type, static_cast<int16_t>(r.cargo), iconL, iconTop,
                        iconL + 0x2C, iconTop + 0x2C, pane, localPlayer);   // sub_458EF0
    }
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
    // Ordem de sub_45A510 (vt 9, 10, 11, 17, 14, 13, 16, 19, 12, 18, +0xA, 15).
    sec.t += drawUpgradesSection(fb, fbW, fbH, sec, r);          // sub_45A690
    sec.t += drawStatusSection(fb, fbW, fbH, sec, r, localPlayer);
    sec.t += drawProductionSection(fb, fbW, fbH, sec, r);
    sec.t += drawResourceSection(fb, fbW, fbH, sec, r);          // sub_45BD50
    sec.t += drawDemandSection(fb, fbW, fbH, sec, r, localPlayer);   // sub_45B540
    sec.t += drawYieldSection(fb, fbW, fbH, sec, unitId);        // sub_45B350
    sec.t += drawWeaponSection(fb, fbW, fbH, sec, r, localPlayer);   // sub_45BBD0
    sec.t += drawObservatorySection(fb, fbW, fbH, sec, r, localPlayer);   // sub_45B440
    sec.t += drawStorageSection(fb, fbW, fbH, sec, r, localPlayer);  // sub_45B070
    sec.t += drawLaunchPadSection(fb, fbW, fbH, sec, r);         // sub_45BE70
    sec.t += 0x0A;
    drawProgressSection(fb, fbW, fbH, sec, r, localPlayer, pane);   // sub_45B850
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
