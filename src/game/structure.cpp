#include "structure.h"

#include "building_sheet.h"
#include "colony.h"
#include "game_map.h"
#include "player.h"
#include "structure_anim.h"
#include "tethys_game.h"
#include "../gfx/artfile.h"

namespace op2 {

static const int kAnimBias = 0x65;   // sub_403E00: `lea eax,[ecx-65h]`

// dword_4DEB24[h + w*4]: animação de explosão de prédio por footprint (valor
// CRU, com o viés 0x65). Lido do OUTPOST2.EXE; só as entradas alcançáveis
// (w 1..5, h 1..4) — no meio da tabela o IDA rotulou a string "Arial".
static int explosionAnimRaw(int w, int h) {
    static const int kTable[25] = {
        0,     0x266, 0x267, 0,     0,      // w=0 (não usado)
        0x6C,  0x59B, 0x47C, 0x47C,         // w=1, h=0..3
        0x708, 0x708, 0x47C, 0x47C,         // w=2
        0x708, 0x70A, 0x70A, 0x709,         // w=3
        0x709, 0x709, 0x70A, 0x70B,         // w=4
        0x70A, 0x70A, 0x70A, 0x70A,         // w=5 (h=4 → índice 24)
    };
    int i = h + w * 4;
    return (i >= 0 && i < 25) ? kTable[i] : 0;
}

// sub_404C70: nFrames-1 da animação CRUA (com viés).
static int animLastFrameRaw(int raw) {
    int real = raw - kAnimBias;
    if (!g_art.ok() || real < 0 || real >= g_art.numAnimations()) return 0;
    int n = static_cast<int>(g_art.animation(real).frames.size());
    return n > 0 ? n - 1 : 0;
}

static bool isEdenTable(const UnitRecord& r) {
    // [unit+0x44] & 0x1000 → tabela A. O port ainda não grava esse bit na
    // criação; ele reflete a facção do dono (IsEden do jogador).
    int ow = r.owner & 0x0F;
    return ow < kMaxPlayers && g_playerData[ow].isEden != 0;
}

// sub_483720 (vt+0x90): animação CRUA do estado, tabela A/B.
static int stateAnimRaw(const UnitRecord& r, int state) {
    int real = structureAnimIndex(r.type, state, isEdenTable(r));
    return real >= 0 ? real + kAnimBias : 0;
}

// sub_4835A0 (vt+0x2C): animação do estado atual.
static int curAnimRaw(const UnitRecord& r) {
    return stateAnimRaw(r, static_cast<int>((r.state34 >> 16) & 0xFFFF));
}

// sub_483A50 (vt+0xC8): nFrames-1 da animação atual; 6 se não há animação.
static int curLastFrame(const UnitRecord& r) {
    int raw = curAnimRaw(r);
    return raw ? animLastFrameRaw(raw) : 6;
}

int structureCurAnim(const UnitRecord& r) {
    int raw = curAnimRaw(r);
    return raw ? raw - kAnimBias : -1;
}

int structureMaxHP(const UnitRecord& r) {
    const StructSheetDesc* d = structSheet(r.type);
    // O engine usa o nibble ALTO de [+0x1D] (sar cl,4); no port o dono.
    return d ? d->player[r.owner & 0x0F].hitPoints : 0;
}

// sub_409220 (vt+0x98): frame do estado atual.
int structureFrame(const UnitRecord& r) {
    const int state = static_cast<int>((r.state34 >> 16) & 0xFFFF);
    const int16_t sub = static_cast<int16_t>(r.state34 & 0xFFFF);
    switch (state) {
        case 0: {
            int k = (r.field22 == 0x22) ? 2 : 1;
            if (sub == 0) return curLastFrame(r);
            int q = r.buildTime >> 2;                              // sar edi,2
            int bx = static_cast<uint16_t>(static_cast<int16_t>(k * sub));  // imul bx,si
            if (q == 0) return 0;
            return curLastFrame(r) * (q - bx) / q;
        }
        case 1: case 2: return 0;
        case 3: return curLastFrame(r) - static_cast<uint16_t>(sub);
        case 5: case 6: case 7: case 8: case 9: return static_cast<uint16_t>(sub);
        default: return 0;
    }
}

// sub_4466B0(0x72, x, y, 6, 0, 0, 1) + sub_405000(anim, div, delay, 0, 0):
// unidade-efeito que toca uma animação e morre sozinha.
static int createAmbientAnim(int pixelX, int pixelY, int animRaw, int div, int delay) {
    Unit u;
    if (!createUnit(u, mapAmbientAnim, LOCATION{pixelX >> 5, pixelY >> 5}, 6)) return 0;
    UnitRecord& e = g_units[u.id()];
    e.owner = static_cast<uint8_t>((e.owner & 0xF6) | 6);       // [+0x1D]
    if (div == 0) div = 1;
    e.effectAnim = (static_cast<uint32_t>(div) << 24) + static_cast<uint32_t>(animRaw);
    e.effectStart = TethysGame::tick() + delay;
    e.flags &= ~UNIT_EFFECT_HIDDEN;                              // arg_C = 0
    // arg_10 = 0: morre ao fim da animação.
    e.action = ACT_DIE; e.field20 = 1;
    e.field3C = animLastFrameRaw(animRaw) * div + delay;
    return u.id();
}

bool effectAnimFrame(const UnitRecord& r, int& anim, int& frame) {
    const int raw = static_cast<int>(r.effectAnim & 0xFFFFFF);
    const int div = static_cast<int8_t>(r.effectAnim >> 24);
    const int elapsed = TethysGame::tick() - r.effectStart;
    if (elapsed < 0 || (r.flags & UNIT_EFFECT_HIDDEN) || div == 0) return false;
    anim = raw - kAnimBias;
    const int nF = animLastFrameRaw(raw) + 1;
    frame = (elapsed / div) % nF;
    return anim >= 0;
}

void killUnit(int id, int delay) {
    UnitRecord& r = g_units[id];
    // Prédio sendo construído/desmontado e ainda no estado 0: o ConVec ([+0x48])
    // tem o dano levado ao máximo e morre também (Kill(5)).
    if ((r.flags & UNIT_BUILDING) && (r.action == ACT_BUILD || r.action == ACT_DISMANTLE)
        && ((r.state34 >> 16) & 0xFFFF) == 0 && r.builderId > 0
        && r.builderId < static_cast<int>(g_units.size())) {
        UnitRecord& cv = g_units[r.builderId];
        const VehPlayerStats* vs = vehicleSheet(cv.type, cv.owner & 0x0F);
        if (vs) cv.damage = static_cast<int16_t>(vs->hitPoints);
        cv.flags |= UNIT_DEAD;
        killUnit(r.builderId, 5);
    }
    r.action = ACT_DIE;
    r.field20 = 1;
    r.field3C = delay;
}

void structureStartBuild(int id, int builderId) {
    UnitRecord& r = g_units[id];
    const StructSheetDesc* d = structSheet(r.type);
    if (!d) return;
    const int pl = r.owner & 0x0F;
    int cvRate = 128, cv3C = 0, cvOwner = pl;
    if (builderId > 0 && builderId < static_cast<int>(g_units.size())) {
        cvOwner = g_units[builderId].owner & 0x0F;
        cv3C = g_units[builderId].field3C;
    }
    // off_4E1348 = descritor do ConVec (map_id 2); bloco do dono do ConVec.
    if (const VehPlayerStats* vs = vehicleSheet(mapConVec, cvOwner)) cvRate = vs->productionRate;
    // imul; cdq; and edx,7Fh; add; sar 7  → divisão por 128 truncando p/ zero.
    int prod = d->player[pl].buildTime * cvRate;
    int dur = (prod + ((prod >> 31) & 0x7F)) >> 7;
    dur += cv3C;
    // sub_4027D0: certos tipos de missão ([dword_4EFD24] em -8..-4) dividem por 2.
    // O port ainda não tem descritor de missão → falso.
    dur &= ~3;
    r.buildTime = dur;
    const int q = dur >> 2;
    if (q <= 0) return;
    const int hp = d->player[pl].hitPoints;
    r.damage = static_cast<int16_t>(((hp - 0x28) / q) * q);    // imul ax,bx
    r.state34 = static_cast<uint32_t>(q & 0xFFFF);             // estado 0, contador q
    r.builderId = builderId;
    r.action = ACT_BUILD; r.field20 = 1; r.field3C = 0;
}

// sub_4831A0(arg=1): tira o prédio do mapa — terraplana o footprint e espalha
// o entulho. (O tratamento da unidade acoplada, as explosões que causam dano
// 0x4A/0x56/0x57 e o som sub_47EE00 dependem de subsistemas ainda não portados.)
static void structureRemoveFromMap(UnitRecord& r, bool destroyed) {
    if (!g_gameMap.ok()) return;
    const StructSheetDesc* d = structSheet(r.type);
    int w = d ? d->sizeX : 1, h = d ? d->sizeY : 1;
    const int mask = g_gameMap.width() - 1;
    // sub_408880: retângulo do footprint.
    const int cx = r.pixelX >> 5, cy = r.pixelY >> 5;
    const int x1 = (cx - (w >> 1)) & mask;
    int y1 = cy - (h >> 1); if (y1 <= 0) y1 = 0;
    const int x2 = (x1 + w - 1) & mask;
    int y2 = y1 + h - 1; if (y2 > g_gameMap.height() - 1) y2 = g_gameMap.height() - 1;

    // Terraplanagem: tile → sub_48B6A0(tile), cellType 0x15.
    for (int x = x1;; x = (x + 1) & mask) {
        for (int y = y1; y <= y2; ++y)
            g_gameMap.setTileAndCellType(x, y, g_gameMap.bulldozedTile(g_gameMap.getTile(LOCATION{x, y})),
                                         GameMap::CELLTYPE_DOZED);
        if (x == x2) break;
    }
    if (!destroyed || !d) return;

    // Entulho: `count` sorteios sobre as células ainda terraplanadas (0x15); a
    // r-ésima delas (varrendo coluna a coluna) vira entulho (0x16).
    auto scatter = [&](int count, bool rare, int& remaining) {
        while (count > 0) {
            int skip = TethysGame::getRand(remaining);          // sub_46EF50(var_18)
            bool placed = false;
            for (int x = x1; !placed;) {
                for (int y = y1; y <= y2; ++y) {
                    if (g_gameMap.getCellType(LOCATION{x, y}) != GameMap::CELLTYPE_DOZED) continue;
                    if (skip) { --skip; continue; }
                    int tile = g_gameMap.getTile(LOCATION{x, y});
                    int rnd = TethysGame::getRand(4);            // dentro de sub_48B7A0/800
                    int nt = rare ? g_gameMap.rareRubbleTile(tile, rnd)
                                  : g_gameMap.commonRubbleTile(tile, rnd);
                    g_gameMap.setTileAndCellType(x, y, nt, GameMap::CELLTYPE_RUBBLE);
                    --count; --remaining; placed = true;
                    break;
                }
                if (x == x2) break;
                x = (x + 1) & mask;
            }
            if (!placed) break;   // (não ocorre: skip < células terraplanadas)
        }
    };
    int remaining = w * h;                                       // var_18
    scatter(d->commonRubble, false, remaining);                  // byte desc+0x2A3
    scatter(d->rareRubble, true, remaining);                     // byte desc+0x2A1
}

// ---- Animações ambiente --------------------------------------------------

// dword_575628 = opção "Ambient" do [Game] do OUTPOST2.INI (padrão 1).
static bool s_optAmbient = true;
void setAmbientAnimations(bool on) { s_optAmbient = on; }

// byte_4E4138: 11 registros de 5 bytes {contagem, ambiente das variantes 1..4};
// 0xFF na contagem = sentinela (registros 0 e 10). Lido do OUTPOST2.EXE.
static const uint8_t kAmbientTable[11][5] = {
    {0xFF, 0, 0, 0, 0},
    {2, 0, 0, 1, 2}, {1, 0, 1, 2, 0}, {3, 0, 1, 0, 1}, {1, 0, 0, 1, 3},
    {0, 0, 1, 1, 2}, {2, 0, 0, 0, 3}, {3, 0, 1, 2, 1}, {1, 0, 1, 2, 0},
    {0, 0, 0, 0, 2},
    {0xFF, 0, 0, 0, 0},
};

// sub_483750 (vt+0xA0): o prédio está operando? Fora de construção/desmontagem,
// dano < 75% do HP, com energia + trabalhadores + cientistas, sem EMP.
bool structureIsOperational(const UnitRecord& r) {
    if (r.action == ACT_BUILD || r.action == ACT_DISMANTLE) return false;
    if ((structureMaxHP(r) * 3 >> 2) <= r.damage) return false;
    const uint32_t need = UNIT_POWERED | UNIT_WORKERS | UNIT_SCIENTISTS;
    if ((r.flags & need) != need) return false;
    return (r.flags & UNIT_DISABLED_TIMERS) == 0;
}

// sub_483810 (vt+0x88): liga trabalhadores (vt+0xAC), cientistas (vt+0xB4) e
// energia (vt+0xB8, que também liga o bit 0).
void structureTurnOn(UnitRecord& r) {
    if (!(r.flags & UNIT_SCIENTISTS)) {
        r.flags |= UNIT_WORKERS;                                 // vt+0xAC
        if (!(r.flags & UNIT_SCIENTISTS)) r.flags |= UNIT_SCIENTISTS;  // vt+0xB4
    }
    if (!(r.flags & UNIT_POWERED)) r.flags |= UNIT_LIGHTS | UNIT_POWERED;  // vt+0xB8
}

// sub_4835C0 (vt+0x94): chamado a cada tick com o prédio intacto e parado no
// estado 0. Age 1x a cada 100 ticks (defasado por y e id); [+0x54] guarda o
// índice em byte_4E4138 e uma contagem regressiva; ao disparar, põe o estado
// 5 + ambiente[índice][variante] (sub_408930 avança o frame até o fim e volta a 0).
static void structureAmbientTick(int id) {
    UnitRecord& r = g_units[id];
    if (!s_optAmbient) return;
    if (!structureOperational(r)) return;                       // vt+0xA0 da classe
    const int variant = static_cast<int8_t>(structureAmbientVariant(r.type, isEdenTable(r)));
    if (variant == 0) return;
    // [+0x14]/[+0x18] no original = centro do tile (no port pixel é o canto).
    const int cx = r.pixelX + 16, cy = r.pixelY + 16, k = id * 8;
    if ((cy + k + TethysGame::tick()) % 100 != 0) return;
    if (r.ambientCursor == 0) {
        int i = (cx + k) % 11;
        if (kAmbientTable[i][0] == 0xFF) i = 1;
        r.ambientCursor = static_cast<uint16_t>(i & 0xFF);
    }
    uint8_t idx = static_cast<uint8_t>(r.ambientCursor & 0xFF);
    uint8_t cd = static_cast<uint8_t>(r.ambientCursor >> 8);
    if (cd) { r.ambientCursor = static_cast<uint16_t>(((cd - 1) << 8) | idx); return; }
    const int amb = static_cast<int8_t>(kAmbientTable[idx][variant]);
    ++idx;
    if (kAmbientTable[idx][0] == 0xFF) idx = 1;
    r.state34 = static_cast<uint32_t>((amb + 5) & 0xFFFF) << 16;
    r.ambientCursor = static_cast<uint16_t>((kAmbientTable[idx][0] << 8) | idx);
}

// sub_408930 (vt+0xC4): estado do prédio pelo dano / início da destruição.
static void structureUpdateState(int id) {
    UnitRecord& r = g_units[id];
    if (r.action == ACT_DIE) {                                   // loc_408A7E
        int cur = curAnimRaw(r), destroyRaw = stateAnimRaw(r, 3);
        if (cur == destroyRaw) return;
        int nFm1 = animLastFrameRaw(destroyRaw);
        r.state34 = 0x30000u | static_cast<uint32_t>(nFm1 & 0xFFFF);
        const StructSheetDesc* d = structSheet(r.type);
        int w = d ? d->sizeX : 1, h = d ? d->sizeY : 1;
        createAmbientAnim(r.pixelX, r.pixelY, explosionAnimRaw(w, h), 2, nFm1 * 2 - 6);
        return;
    }
    if (r.action == ACT_BUILD || r.action == ACT_DISMANTLE) return;
    const int hp = structureMaxHP(r);
    const int dmg = r.damage;
    // As transições 1→2 e (0/≥3)→1 tocam um alerta, sub_47EE00(unk_56C610, x, y,
    // 2 ou 1) — gerenciador de som, ainda não portado.
    if (hp * 6 / 10 <= dmg) { r.state34 = 0x20000; return; }  // dano >= 60%
    if (hp * 3 / 10 <= dmg) { r.state34 = 0x10000; return; }  // dano >= 30%
    const uint32_t state = (r.state34 >> 16) & 0xFFFF;
    if (state == 1 || state == 2) { r.state34 = 0; return; }     // reparado
    if (state == 0) { structureAmbientTick(id); return; }        // vt+0x94
    if (state < 5) return;
    uint32_t sub = (r.state34 & 0xFFFF) + 1;
    if (static_cast<uint32_t>(curLastFrame(r)) < sub) r.state34 = 0;
    else r.state34 = (state << 16) | sub;
}

// sub_43ACA0 (vt[1]) — só os casos que prédios e efeitos usam.
static void actionTick(int id) {
    UnitRecord& r = g_units[id];
    if (r.field20) {
        if (r.action == ACT_DIE) {
            if (r.field3C <= 0 || r.field3C > 10000) r.field3C = 10;
            r.field22 = SUB_WAIT; r.field20 = 0;
            return;
        }
        if (r.field3C) { r.field22 = 0; --r.field3C; return; }  // loc_43AD29
        r.field23 = 0;
        switch (r.action) {                                     // jpt_43AD56
            case ACT_BUILD:                                     // case 33
                r.field22 = SUB_WAIT;
                r.field3C = 4;      // 1 se dword_56DE6C (atalho de debug)
                break;
            case ACT_DISMANTLE:                                 // case 34
                r.field22 = 0; r.state34 = 0;
                break;
            default: break;
        }
        r.field20 = 0;
        return;
    }
    if (r.field22 != 0) return;                                 // loc_43B2ED
    switch (r.action) {                                         // jpt_43B312
        case ACT_BUILD: {                                       // case 33
            uint16_t sub = static_cast<uint16_t>(r.state34 & 0xFFFF);
            if (sub) {
                --sub;
                r.state34 = (r.state34 & 0xFFFF0000u) | sub;
                int q = r.buildTime >> 2;
                if (q) r.damage = static_cast<int16_t>(r.damage - (structureMaxHP(r) - 0x28) / q);
                r.field22 = SUB_WAIT;
                r.field3C = 4;
            } else {
                // Pronto: [+0x21]=0 e vt+0x8C (sub_483850: marca a economia do dono
                // p/ recálculo). Estatísticas e a mensagem "%s completed"
                // (sub_438F70) — ainda não portadas.
                r.action = 0;
                colonyMarkDirty(r.owner & 0x0F);
            }
            break;
        }
        case ACT_DIE:                                           // case 47
            if (r.flags & UNIT_BUILDING) {
                uint16_t sub = static_cast<uint16_t>(r.state34 & 0xFFFF);
                if (sub) {
                    --sub;
                    r.state34 = (r.state34 & 0xFFFF0000u) | sub;
                    r.field22 = SUB_WAIT;
                    r.field3C = 1;                              // loc_43DCDF
                } else {
                    r.field22 = SUB_REMOVE;
                    structureRemoveFromMap(r, true);            // vt+0x80(1)
                }
            } else if (!(r.flags & UNIT_VEHICLE)) {
                r.field22 = SUB_REMOVE;                         // loc_43DB49
            }
            break;
        default: break;
    }
}

// sub_408910 (vt[2] do prédio) / sub_43DE50 (vt[2] do efeito). Retorna true
// se a unidade deve ser removida.
static bool updateTick(int id) {
    UnitRecord& r = g_units[id];
    if ((r.flags & UNIT_BUILDING) && r.action != ACT_BUILD) structureUpdateState(id);
    UnitRecord& u = g_units[id];          // (structureUpdateState pode criar unidades)
    switch (u.field22) {
        case SUB_WAIT: case 9:            // loc_43E2B3
            if (--u.field3C == 0) { u.field23 = u.field22; u.field22 = 0; }
            return false;
        case SUB_REMOVE:                  // loc_43E77B
            return true;
        default:
            return false;
    }
}

void removeUnit(int id) {
    UnitRecord& r = g_units[id];
    if (r.flags & UNIT_BUILDING) colonyStructureRemoved(id);    // sub_439C00
    if (g_gameMap.ok()) {
        int cx = r.pixelX >> 5, cy = r.pixelY >> 5;
        if (r.flags & UNIT_BUILDING) {
            int w, h; buildingFootprint(r.type, w, h);
            int tlx = cx - (w >> 1), tly = cy - (h >> 1);
            for (int dy = 0; dy < h; ++dy)
                for (int dx = 0; dx < w; ++dx)
                    if (g_gameMap.cellOccupant(tlx + dx, tly + dy) == id)
                        g_gameMap.clearUnitOccupancy(tlx + dx, tly + dy);
        } else if (r.flags & UNIT_VEHICLE) {
            if (g_gameMap.cellOccupant(cx, cy) == id) g_gameMap.clearUnitOccupancy(cx, cy);
        }
    }
    r = UnitRecord{};    // id = -1: slot livre
}

void tickUnits() {
    // Itera por id (no original, a lista encadeada dword_54EC0C). Unidades
    // criadas durante o laço entram no fim e já rodam neste tick, como na lista.
    for (int id = 1; id < static_cast<int>(g_units.size()); ++id) {
        if (g_units[id].id == -1 || (g_units[id].flags & UNIT_VEHICLE)) continue;
        actionTick(id);
        if (updateTick(id)) removeUnit(id);
    }
}

} // namespace op2
