#include "salvage.h"

#include <cstdlib>

#include "building_sheet.h"
#include "docking.h"
#include "game_map.h"
#include "messages.h"
#include "mining.h"
#include "movement.h"
#include "pathfinder.h"
#include "player.h"
#include "research.h"
#include "structure.h"
#include "tethys_game.h"
#include "trigger.h"
#include "unit.h"
#include "weapon.h"

namespace op2 {

static bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }
static int statsOf(const UnitRecord& r) { return (r.owner >> 4) & 0x0F; }
static int tileMask() { return g_gameMap.width() - 1; }
static int tileX(const UnitRecord& r) { return (unitOX(r) >> 5) & tileMask(); }
static int tileY(const UnitRecord& r) { return unitOY(r) >> 5; }
static bool sameTile(const UnitRecord& a, const UnitRecord& b) {
    return tileX(a) == tileX(b) && tileY(a) == tileY(b);
}
// vt[0x74] (aceita comando).
static bool commandable(const UnitRecord& r) {
    return r.id != -1 && !((r.flags | (r.flags << 2)) & 0x80000u) && r.field22 != 0x0A;
}

// ---- Destroço ----------------------------------------------------------------
void wreckDiscover(int id, int p) {                          // vt[0x98] sub_405B10
    UnitRecord& w = g_units[id];
    const uint32_t old = w.wreckMask;
    w.wreckMask |= 1u << p;
    if (old == w.wreckMask || p != TethysGame::localPlayer()) return;
    postMessage(unitOX(w), unitOY(w), "Wreckage Discovered", TethysGame::tick() > 0 ? 0x97 : 0);
}

bool wreckKnownBy(const UnitRecord& w, int p) {              // vt[0x9C] sub_405B80
    for (int i = 0; i < TethysGame::noPlayers(); ++i)
        if ((w.wreckMask >> i & 1u) && (g_playerData[i].allyMask >> p & 1u)) return true;
    return false;
}

static int wreckAtTile(const UnitRecord& u) {                // lista de efeitos dword_5737D0
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
        const UnitRecord& w = g_units[i];
        if (w.id != -1 && w.type == 0x54 && !(w.flags & UNIT_DEAD) && sameTile(w, u)) return i;
    }
    return 0;
}

bool wreckTick(int id) {                                     // sub_405BF0
    UnitRecord& w = g_units[id];
    if (w.field22 == SUB_REMOVE) return true;
    const int wx = tileX(w), wy = tileY(w);
    if ((g_gameMap.rawCell(wx, wy) & 0x1F) == 0x15) return true;   // terraplanado
    // sub_437A90(x, y, 0xA0): veículos/prédios (0x46) de qualquer dono a < 160 px.
    const int px = unitOX(w), py = unitOY(w), r = 0xA0;
    const int W = g_gameMap.width() * 32;
    for (int i = unitListFirst(); i; i = unitListNext(i)) {
        const UnitRecord& t = g_units[i];
        const int uy = unitOY(t);
        if (uy < py - r) continue;
        if (uy >= py + r) break;
        if (!(t.flags & 0x46u) || t.field22 == 0x0D || t.id == -1 || (t.flags & UNIT_DEAD)) continue;
        int dx = std::abs(px - unitOX(t));
        if (dx > W / 2) dx = W - dx;
        const int dy = uy - py;
        if (static_cast<unsigned>(dx * dx + dy * dy) >= static_cast<unsigned>(r * r)) continue;
        if (t.type == mapScout) wreckDiscover(id, ownerOf(t));
        if ((t.flags & UNIT_BUILDING) && sameTile(t, g_units[id])) return true;
    }
    return false;
}

bool wreckSprite(int id, int& animRaw, int& frame) {         // sub_405A50
    const UnitRecord& w = g_units[id];
    const int me = TethysGame::localPlayer();
    if (!(g_playerData[me].allyMask & w.wreckMask)) return false;
    animRaw = 0x7A5;
    const int n = artLastFrameRaw(0x7A5);                     // (divide pelo último frame)
    frame = n > 0 ? static_cast<int>(TethysGame::tick() % static_cast<uint32_t>(n)) : 0;
    return true;
}

// ---- Entulho ----------------------------------------------------------------
// sub_48BB80 / sub_48BBD0: faixas [+6, +6+4) e [+6+4, +6+8) de algum tipo de
// terreno (a 1ª também aceita o tile 0x35).
static bool rubbleTile(int tile, bool second) {
    for (const auto& t : g_gameMap.terrainTypes()) {
        const int w6 = t.word(6);
        const int lo = second ? w6 + 4 : w6, hi = second ? w6 + 8 : w6 + 4;
        if (tile >= lo && tile < hi) return true;
    }
    return !second && tile == 0x35;
}

void gorfReceiveRubble(int gorfId, int kind, int amount) {   // sub_46E4E0 / sub_46E550
    const UnitRecord& g = g_units[gorfId];
    PlayerData& pd = g_playerData[ownerOf(g)];
    const uint8_t* blk = unitStatBlock(g.type, statsOf(g));
    const bool improved = blk && blk[0x1D];
    if (kind == 6) { pd.ore += amount; if (improved) pd.ore += 0x19; }
    else           { pd.rareOre += amount; if (improved) pd.rareOre += 0x0F; }
}

// ---- Beacon ------------------------------------------------------------------
void beaconSurveyBy(int beaconId, int p) {                   // vt[0x9C] sub_405420
    UnitRecord& b = g_units[beaconId];
    const uint32_t bit = 1u << p;
    if (b.surveyedMask & bit) return;
    if (b.beaconRare && !researchHas(p, researchTopicIndex(0x16))) return;   // vt[0x98]: raro
    const uint32_t before = g_playerData[p].allyMask & b.surveyedMask;
    b.surveyedMask |= bit;
    if (p != TethysGame::localPlayer() || before) return;
    for (size_t i = 1; i < g_units.size(); ++i)               // sub_490800(0x58): EDWARD
        if (g_units[i].id != -1 && g_units[i].type == 0x58 && ownerOf(g_units[i]) == p) return;
    if (TethysGame::tick() > 0) postMessage(unitOX(b), unitOY(b), "Mining Beacon Surveyed", 0x9A);
}

static int beaconAtTile(const UnitRecord& u) {
    for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
        const UnitRecord& b = g_units[i];
        if (b.id != -1 && b.type == 0x51 && sameTile(b, u)) return i;
    }
    return 0;
}

// sub_44BE40: sai de cima — (x, y−1), senão (x, y+1), se enteráveis.
static void stepOff(int id) {
    const UnitRecord& u = g_units[id];
    const int x = tileX(u);
    int y = tileY(u) - 1;
    pfPush();
    pfSetContext(id);
    bool ok = pfCanEnter(x, y) != 0;
    if (!ok) { y += 2; ok = pfCanEnter(x, y) != 0; }
    if (ok && unitStepToward(id, x, y)) g_units[id].field22 = 1;   // vt[0x7C] + [+0x22] = 1
    pfPop();
}

// ---- Chegada e ações 0x0F / 0x11 ----------------------------------------------
void unitArrived(int id) {
    UnitRecord& u = g_units[id];
    if (u.flags & 0x40u) return;
    if (specialTargetCheck(id)) return;                       // sub_44D5B0
    if (u.type == mapCargoTruck && u.cargo == 0) {            // sub_406C20
        UnitRecord& t = g_units[id];
        const bool rubble = (g_gameMap.rawCell(tileX(t), tileY(t)) & 0x1F) == 0x16;
        bool go = rubble;
        if (!go) {
            for (int i = 1; i < static_cast<int>(g_units.size()) && !go; ++i) {
                const UnitRecord& w = g_units[i];
                if (w.id != -1 && w.type == 0x54 && sameTile(w, t) && wreckKnownBy(w, ownerOf(t))) go = true;
            }
        }
        if (go) { t.action = 0x0F; t.field20 = 1; t.field3C = 0; }
    }
    UnitRecord& s = g_units[id];
    if (s.type == mapRoboSurveyor && beaconAtTile(s)) {       // sub_484460
        s.action = 0x11; s.field20 = 1; s.field3C = 0;
    }
}

void salvageActionStart(int id) {                            // jpt_43AD56 cases 15/17
    UnitRecord& u = g_units[id];
    u.field22 = SUB_WAIT;
    u.field3C = 0x1E;
    u.field20 = 0;
}

bool salvageActionTick(int id) {
    UnitRecord& u = g_units[id];
    if (u.action == 0x0F) {                                   // jpt_43B312 case 15
        const int x = tileX(u), y = tileY(u);
        const uint32_t cell = g_gameMap.rawCell(x, y);
        const int tile = static_cast<int>((cell >> 5) & 0x7FF);
        int kind = 0, amount = 0;
        if (rubbleTile(tile, false)) { kind = 6; amount = 0xD7; }
        else if (rubbleTile(tile, true)) { kind = 7; amount = 0xFA; }
        else if (const int w = wreckAtTile(u)) {
            kind = 9;
            amount = g_units[w].wreckTech - 0x1F40;
            g_units[w].field22 = SUB_REMOVE;
        }
        truckSetCargo(id, kind, amount, 0);                   // vt[0x94]
        UnitRecord& t = g_units[id];
        if (kind != 9) {                                      // entulho → terraplanado
            g_gameMap.setTileAndCellType(x, y, g_gameMap.bulldozedTile(tile), 0x15);
        } else if (ownerOf(t) == TethysGame::localPlayer()) {
            postMessage(unitOX(t), unitOY(t), "Wreckage Loaded", 0xB0);
        }
        t.action = 0;
        return true;
    }
    if (u.action == 0x11) {                                   // jpt_43B312 case 17
        if (const int b = beaconAtTile(u)) beaconSurveyBy(b, ownerOf(u));
        g_units[id].action = 0;
        stepOff(id);
        return true;
    }
    return false;
}

// ---- Spaceport: baia ↔ caminhão (sub_47F470, flag 0) ---------------------------
bool spaceportBayTransfer(int owner, int sp, int bay) {
    if (!valid(sp) || bay < 0 || bay > 5) return false;
    UnitRecord& s = g_units[sp];
    if (ownerOf(s) != (owner & 0x0F) || (s.flags & UNIT_DEAD)) return false;
    const int v = dockedVehicle(sp);
    if (!v) return false;
    UnitRecord& t = g_units[v];
    if ((t.flags & 0x40000u) || !commandable(t)) return false;
    const uint16_t cargo = t.cargo;
    const uint8_t bit = static_cast<uint8_t>(1u << bay);
    const int8_t inBay = static_cast<int8_t>(s.bayType[bay]);
    if ((cargo & 0x0F) == 9) {                                // destroço → pesquisa
        const int tech = ((cargo >> 4) & 0xFFF) + 0x1F40;
        researchMarkComplete(ownerOf(s), techIndex(tech));    // sub_472CD0 + sub_473830
        if (inBay) truckSetCargo(v, 8, inBay, 0); else truckSetCargo(v, 0, 0, 0);
        UnitRecord& t2 = g_units[v];
        t2.flags = (t2.flags & ~0x200000u) | ((static_cast<uint32_t>(s.bayBits & bit) << 21) & 0x200000u);
        s.bayType[bay] = 0;
        return true;
    }
    const int16_t item = static_cast<int16_t>(cargo) >> 4;
    const bool vehBit = (t.flags & 0x200000u) != 0;
    if (inBay) truckSetCargo(v, 8, inBay, 0); else truckSetCargo(v, 0, 0, 0);
    UnitRecord& t2 = g_units[v];
    t2.flags = (t2.flags & ~0x200000u) | ((static_cast<uint32_t>(s.bayBits & bit) << 21) & 0x200000u);
    if (!vehBit) s.bayBits = 0;
    s.bayType[bay] = static_cast<uint8_t>(item);
    return true;
}

} // namespace op2
