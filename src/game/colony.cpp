#include "colony.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "building_sheet.h"
#include "game_map.h"
#include "structure.h"
#include "tethys_game.h"
#include "../res/res_manager.h"

namespace op2 {

ColonyState g_colony[kMaxPlayers];
static std::vector<int> s_structList[kMaxPlayers];

static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }

void colonyMarkDirty(int player) {
    if (player >= 0 && player < kMaxPlayers) g_colony[player].dirty = 1;
}

void colonyReset() {
    for (int p = 0; p < kMaxPlayers; ++p) { s_structList[p].clear(); g_colony[p] = ColonyState{}; }
}

const std::vector<int>& colonyStructures(int player) {
    static const std::vector<int> kEmpty;
    return (player >= 0 && player < kMaxPlayers) ? s_structList[player] : kEmpty;
}

static int resourcePriority(int type) {
    const StructSheetDesc* d = structSheet(type);
    return d ? d->resourcePriority : 0;               // byte desc+0x2A0
}

// sub_439FE0 (ramo de prédio): marca dirty e insere por prioridade.
void colonyStructureAdded(int id) {
    const UnitRecord& r = g_units[id];
    const int p = ownerOf(r);
    if (p >= kMaxPlayers) return;
    g_colony[p].dirty = 1;
    std::vector<int>& L = s_structList[p];
    const int prio = resourcePriority(r.type);
    size_t pos = L.size();
    for (size_t i = 0; i < L.size(); ++i)
        if (resourcePriority(g_units[L[i]].type) >= prio) { pos = i; break; }
    L.insert(L.begin() + static_cast<long>(pos), id);
}

// sub_439C00 (ramo de prédio): marca dirty e retira da lista.
void colonyStructureRemoved(int id) {
    const UnitRecord& r = g_units[id];
    const int p = ownerOf(r);
    if (p >= kMaxPlayers) return;
    g_colony[p].dirty = 1;
    std::vector<int>& L = s_structList[p];
    for (size_t i = 0; i < L.size(); ++i)
        if (L[i] == id) { L.erase(L.begin() + static_cast<long>(i)); break; }
}

// ---- MORALE.TXT: eventos e condições (tabela @0x4E1F08, 67 x {nome, valor,
// flag %}). sub_44E180 exige todas; flag 1 → valor convertido p/ /256.
enum MoraleEntry {
    ME_KID_DIES, ME_ADULT_DIES, ME_KID_BORN, ME_GOOD_BUILD_DIES, ME_REG_BUILD_DIES,
    ME_NEW_TECH_BORN, ME_DISASTER_NO_WARN, ME_DISASTER_WARNED, ME_CONSUMER_GOODS_1,
    ME_CONSUMER_GOODS_2, ME_CONSUMER_GOODS_3, ME_ENEMY_GOOD_DIES, ME_ENEMY_BAD_DIES,
    ME_ENEMY_VEH_DIES, ME_TECH_SCHOOL, ME_PHD_TRAINED, ME_CC_BORN, ME_EVENT_DEC_RATE,
    ME_CROWDED_NOT, ME_CROWDED_LOW, ME_CROWDED_MED, ME_CROWDED_HIGH, ME_CROWDED_MAX,
    ME_FOOD_SURPLUS, ME_FOOD_DEFICIT_BIGSUPPLY, ME_FOOD_DEFICIT, ME_FOOD_STARVING,
    ME_DIS_BLD_LOW, ME_DIS_BLD_MED, ME_DIS_BLD_HIGH, ME_DIS_BLD_MAX,
    ME_REC_UT_LOW, ME_REC_UT_MED, ME_REC_UT_HIGH, ME_REC_UT_MAX,
    ME_FORUM_UT_LOW, ME_FORUM_UT_MED, ME_FORUM_UT_HIGH, ME_FORUM_UT_MAX,
    ME_MED_UT_LOW, ME_MED_UT_MED, ME_MED_UT_HIGH, ME_MED_UT_MAX,
    ME_NURSERY_ON, ME_UNIV_ON, ME_DIS_MET_ON, ME_DIS_VOL_ON, ME_DIS_SAND_ON,
    ME_DIS_QUAKE_ON, ME_DIS_ELEC_ON, ME_DIRT_LOW, ME_DIRT_MED, ME_DIRT_HIGH, ME_DIRT_MAX,
    ME_UNOC_LOW, ME_UNOC_MED, ME_UNOC_HIGH, ME_PHD_WHINE_LOW, ME_PHD_WHINE_MED,
    ME_PHD_WHINE_HIGH, ME_PHD_WHINE_MAX, ME_DIFF_EASY, ME_DIFF_MED, ME_DIFF_HARD,
    ME_DIFF_EASY_PLY, ME_DIFF_MED_PLY, ME_DIFF_HARD_PLY, ME_COUNT
};
static const char* const kMoraleEntryName[ME_COUNT] = {
    "KID_DIES", "ADULT_DIES", "KID_BORN", "GOOD_BUILD_DIES", "REG_BUILD_DIES",
    "NEW_TECH_BORN", "DISASTER_NO_WARN", "DISASTER_WARNED", "CONSUMER_GOODS_1",
    "CONSUMER_GOODS_2", "CONSUMER_GOODS_3", "ENEMY_GOOD_DIES", "ENEMY_BAD_DIES",
    "ENEMY_VEH_DIES", "TECH_SCHOOL", "PHD_TRAINED", "CC_BORN", "EVENT_DEC_RATE",
    "CROWDED_NOT", "CROWDED_LOW", "CROWDED_MED", "CROWDED_HIGH", "CROWDED_MAX",
    "FOOD_SURPLUS", "FOOD_DEFICIT_BIGSUPPLY", "FOOD_DEFICIT", "FOOD_STARVING",
    "DIS_BLD_LOW", "DIS_BLD_MED", "DIS_BLD_HIGH", "DIS_BLD_MAX",
    "REC_UT_LOW", "REC_UT_MED", "REC_UT_HIGH", "REC_UT_MAX",
    "FORUM_UT_LOW", "FORUM_UT_MED", "FORUM_UT_HIGH", "FORUM_UT_MAX",
    "MED_UT_LOW", "MED_UT_MED", "MED_UT_HIGH", "MED_UT_MAX",
    "NURSERY_ON", "UNIV_ON", "DIS_MET_ON", "DIS_VOL_ON", "DIS_SAND_ON",
    "DIS_QUAKE_ON", "DIS_ELEC_ON", "DIRT_LOW", "DIRT_MED", "DIRT_HIGH", "DIRT_MAX",
    "UNOC_LOW", "UNOC_MED", "UNOC_HIGH", "PHD_WHINE_LOW", "PHD_WHINE_MED",
    "PHD_WHINE_HIGH", "PHD_WHINE_MAX", "DIFF_EASY", "DIFF_MED", "DIFF_HARD",
    "DIFF_EASY_PLY", "DIFF_MED_PLY", "DIFF_HARD_PLY",
};
static int s_me[ME_COUNT];
static bool isPctEntry(int i) { return i == ME_EVENT_DEC_RATE || i >= ME_DIFF_EASY; }
// Somas pré-calculadas pelo loader (objeto de moral +0xF0..+0x104).
static int s_meBaseMax = 0, s_meMedSum = 0, s_meRecSum = 0, s_meDirtSum = 0;
static const int kMoraleScale = 100;            // +0x104

static void parseMoraleEntries(const std::string& text) {
    size_t p = 0;
    while (p < text.size()) {
        size_t q = text.find('\n', p);
        std::string line = text.substr(p, q == std::string::npos ? std::string::npos : q - p);
        p = (q == std::string::npos) ? text.size() : q + 1;
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos || t1 == 0) continue;
        std::string name = line.substr(0, t1);
        for (int i = 0; i < ME_COUNT; ++i) {
            if (name != kMoraleEntryName[i]) continue;
            // coluna 1 vazia; coluna 2 = valor (sub_4112F0 duas vezes)
            size_t t2 = line.find('\t', t1 + 1);
            int v = (t2 == std::string::npos) ? 0 : std::atoi(line.c_str() + t2 + 1);
            s_me[i] = isPctEntry(i) ? (v * 256 + 50) / 100 : v;
            break;
        }
    }
    s_meBaseMax = s_me[ME_DIS_BLD_LOW] + s_me[ME_FOOD_SURPLUS] + s_me[ME_CROWDED_NOT];
    s_meMedSum = s_me[ME_MED_UT_LOW] + s_me[ME_MED_UT_MED] + s_me[ME_MED_UT_HIGH] + s_me[ME_MED_UT_MAX];
    s_meRecSum = s_me[ME_REC_UT_LOW] + s_me[ME_REC_UT_MED] + s_me[ME_REC_UT_HIGH] + s_me[ME_REC_UT_MAX];
    s_meDirtSum = s_me[ME_DIRT_LOW] + s_me[ME_DIRT_MED] + s_me[ME_DIRT_HIGH] + s_me[ME_DIRT_MAX];
}

// ---- MORALE.TXT ------------------------------------------------------------
static MoraleRow s_morale[5];

bool loadMoraleSheet() {
    auto stream = g_ResManager.openStream("morale.txt");
    if (!stream) { std::cerr << "[Sheets] MORALE.TXT nao encontrado" << std::endl; return false; }
    std::string text(reinterpret_cast<const char*>(stream->data()), stream->size());
    // 1ª linha = cabeçalho (8 colunas); depois 5 linhas de nível.
    size_t p = text.find('\n');
    for (int lv = 0; lv < 5 && p != std::string::npos; ++lv) {
        size_t q = text.find('\n', p + 1);
        std::string line = text.substr(p + 1, q == std::string::npos ? std::string::npos : q - p - 1);
        p = q;
        int v[8] = {0};
        size_t a = line.find('\t');                       // pula o nome
        for (int k = 0; k < 8 && a != std::string::npos; ++k) {
            v[k] = std::atoi(line.c_str() + a + 1);
            a = line.find('\t', a + 1);
        }
        auto pct = [](int x) { return (x * 256 + 50) / 100; };   // shl 8; +32h; idiv 64h
        MoraleRow& m = s_morale[lv];
        m.points = v[0];
        m.powBonus = pct(v[1]); m.rschBonus = pct(v[2]);
        m.factBonus = pct(v[3]); m.foodBonus = pct(v[4]);
        m.defectRate = v[5]; m.fertRate = v[6]; m.dieRate = v[7];
    }
    parseMoraleEntries(text);
    return true;
}

const MoraleRow& moraleRow(int level) {
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    return s_morale[level];
}

// ---- Métodos por classe (vtables de estrutura) ------------------------------
static const StructPlayerStats* stats(const UnitRecord& r) {
    const StructSheetDesc* d = structSheet(r.type);
    return d ? &d->player[ownerOf(r)] : nullptr;
}
static bool healthy(const UnitRecord& r) {               // hp*3>>2 > dano
    const StructPlayerStats* s = stats(r);
    return s && ((s->hitPoints * 3) >> 2) > r.damage;
}
static bool isPowerPlant(int t) {
    return t == 0x1D /*ASE*/ || t == 0x2C /*GEOTHERMAL*/ || t == 0x3A /*TOKAMAK*/;
}
static bool isLab(int t) { return t == 0x21 || t == 0x31 || t == 0x32; }
static const int kUniversity = 0x26;
static const int kSolar = 0x24;

bool structureOperational(const UnitRecord& r) {
    const int t = r.type;
    if (t == mapCommandCenter) {                          // sub_40C1B0
        if (r.action == ACT_BUILD || r.action == ACT_DISMANTLE || !healthy(r)) return false;
        if (r.flags & 0x40000) return false;
        return (r.flags & UNIT_WORKERS) != 0;
    }
    if (isPowerPlant(t) || t == kSolar) {                 // sub_46E6D0 / sub_46E880
        if (r.action == ACT_BUILD || r.action == 0x12 || r.action == ACT_DISMANTLE) return false;
        if (!healthy(r)) return false;
        return (r.flags & (t == kSolar ? UNIT_SOLAR_IDLE : 0x40000u)) == 0;
    }
    if (t == 0x15 || t == 0x16 || t == 0x2A) {           // sub_44AD60 (minas, Magma Well)
        if (r.action == ACT_BUILD || !healthy(r)) return false;
        const uint32_t need = UNIT_POWERED | UNIT_WORKERS | UNIT_SCIENTISTS;
        return (r.flags & need) == need && !(r.flags & 0x40000);
    }
    return structureIsOperational(r);                     // sub_483750
}

// vt+0xA4: o gerador está produzindo? (base: 0). Solar (sub_46E7F0) consome um
// satélite do jogador por array ativo.
static bool producing(UnitRecord& r, ColonyState& c) {
    const int t = r.type;
    if (isPowerPlant(t)) return structureOperational(r);  // sub_46E720
    if (t != kSolar) return false;
    if (r.action == ACT_BUILD || r.action == 0x12 || r.action == ACT_DISMANTLE ||
        !healthy(r) || (r.flags & 0x40000) || c.solarSatellites == 0) {
        r.flags |= UNIT_SOLAR_IDLE;
        return false;
    }
    --c.solarSatellites;
    r.flags &= ~UNIT_SOLAR_IDLE;
    return true;
}

// vt+0xD0: energia gerada = Production Capacity + bônus de moral (POW).
static int powerProduced(const UnitRecord& r) {
    const int t = r.type;
    if (!(isPowerPlant(t) || t == kSolar || t == mapCommandCenter)) return 0;
    const StructPlayerStats* s = stats(r);
    if (!s) return 0;
    const int cap = s->productionCapacity;               // bloco+0x34
    const int bonus = moraleRow(g_playerData[ownerOf(r)].morale).powBonus;
    return cap + ((cap * bonus + 0x7F) >> 8);
}

static int powerRequired(const UnitRecord& r) {          // bloco+0x24
    const StructPlayerStats* s = stats(r); return s ? s->power : 0;
}
static int workersRequired(const UnitRecord& r) {        // vt+0xE8 (Univ: +[+0x60])
    const StructPlayerStats* s = stats(r);
    return (s ? s->workers : 0) + (r.type == kUniversity ? r.extraWorkers : 0);
}
static int scientistsRequired(const UnitRecord& r) {     // vt+0xE0 (labs: +[+0x64])
    const StructPlayerStats* s = stats(r);
    return (s ? s->scientists : 0) + (isLab(r.type) ? r.extraScientists : 0);
}
static int researchAssigned(const UnitRecord& r) {       // vt+0xE4
    return isLab(r.type) ? r.extraScientists : 0;
}

// ---- Rede de tubos ----------------------------------------------------------
// Componente = prédios do MESMO dono ligados por caminhos 4-conexos que passam
// pelos footprints deles e por células de tubo (cellType 0x1A); prédio de outro
// dono bloqueia; veículo sobre o tubo não. É o que a rede incremental do
// original mantém (dword_547350: sub_42F000/42F4A0/42F8A0, flood fill de
// Heckbert sub_4362D0 com os predicados sub_42EC40/42ED40/42EED0). Aqui ela é
// recalculada a cada alocação — mesmos componentes, sem o pool de nós.
static const int kTubeCellType = 0x1A;

static void computeComponents(int player, std::vector<int>& comp) {
    comp.assign(g_units.size(), -1);
    if (!g_gameMap.ok()) return;
    const int W = g_gameMap.width(), H = g_gameMap.height();
    std::vector<uint8_t> seen(static_cast<size_t>(W) * H, 0);
    int next = 0;
    std::vector<int> stack;
    for (int sid : s_structList[player]) {
        if (comp[sid] >= 0) continue;
        const int cid = next++;
        comp[sid] = cid;
        stack.clear();
        auto pushFootprint = [&](int id) {
            const UnitRecord& u = g_units[id];
            int w, h; buildingFootprint(u.type, w, h);
            int tlx = (u.pixelX >> 5) - (w >> 1), tly = (u.pixelY >> 5) - (h >> 1);
            for (int dy = 0; dy < h; ++dy)
                for (int dx = 0; dx < w; ++dx) {
                    int x = (tlx + dx) & (W - 1), y = tly + dy;
                    if (y < 0 || y >= H) continue;
                    size_t k = static_cast<size_t>(y) * W + x;
                    if (!seen[k]) { seen[k] = 1; stack.push_back(static_cast<int>(k)); }
                }
        };
        pushFootprint(sid);
        while (!stack.empty()) {
            int k = stack.back(); stack.pop_back();
            int x = k % W, y = k / W;
            static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
            for (auto& d : d4) {
                int nx = (x + d[0]) & (W - 1), ny = y + d[1];
                if (ny < 0 || ny >= H) continue;
                size_t nk = static_cast<size_t>(ny) * W + nx;
                if (seen[nk]) continue;
                int occ = g_gameMap.cellOccupant(nx, ny);
                if (occ > 0 && occ < static_cast<int>(g_units.size()) &&
                    (g_units[occ].flags & UNIT_BUILDING)) {
                    const UnitRecord& o = g_units[occ];
                    if (ownerOf(o) != player || comp[occ] >= 0) continue;
                    comp[occ] = cid;                       // mesmo dono: junta
                    pushFootprint(occ);
                    continue;
                }
                if (g_gameMap.getCellType(LOCATION{nx, ny}) == kTubeCellType) {
                    seen[nk] = 1;
                    stack.push_back(static_cast<int>(nk));
                }
            }
        }
    }
}

// ---- vt+0x9C: estatísticas por classe ----------------------------------------
// Grupo de 4 contadores (total / operando / parado / inativo) — sub_401CD0 e
// cópias por classe (sub_446D40, sub_449530, sub_409A20...).
static void statGroup(ColonyState& c, int off, const UnitRecord& r, bool inactive) {
    ++c.stats[off / 4];
    if (inactive) ++c.stats[(off + 0x0C) / 4];
    else if (structureOperational(r)) ++c.stats[(off + 0x04) / 4];
    else ++c.stats[(off + 0x08) / 4];
}

static int prodCap(const UnitRecord& r) {
    const StructPlayerStats* s = stats(r); return s ? s->productionCapacity : 0;
}

static void statsTick(ColonyState& c, const UnitRecord& r, bool inactive) {
    const bool op = structureOperational(r);
    switch (r.type) {
        case 0x27: statGroup(c, ST_AGRI_GRP, r, inactive); break;    // sub_401CD0 (base)
        case 0x1C:                                                     // sub_40C140 (CC)
        case 0x1D: case 0x24: case 0x2C: case 0x3A:                    // sub_446D40
            statGroup(c, ST_POWER_GRP, r, inactive);
            if (r.type == 0x1C && op) ++c.stats[ST_CC_OP / 4];
            break;
        case 0x1F: statGroup(c, ST_ROBOT_GRP, r, inactive); break;    // sub_40C270
        case 0x36: statGroup(c, ST_SMELT_GRP, r, inactive); break;    // sub_449530
        case 0x38: statGroup(c, ST_RSMELT_GRP, r, inactive); break;   // sub_449630
        case 0x39: statGroup(c, ST_GORF_GRP, r, inactive); break;     // sub_449E40
        case 0x19: statGroup(c, ST_STORE_GRP, r, inactive); break;    // sub_409A20
        case 0x1A: statGroup(c, ST_RSTORE_GRP, r, inactive); break;   // sub_409A70
        case 0x15: case 0x16: {                                        // sub_44B3B0 / sub_447E90
            const int base = r.type == 0x15 ? ST_MINE : ST_RMINE;
            ++c.stats[base / 4];
            if (r.mineYield >= 0 && r.mineYield <= 2) ++c.stats[(base + 4 + r.mineYield * 4) / 4];
            break;
        }
        case 0x25: if (op) c.stats[ST_REC_CAP / 4] += prodCap(r); break;      // sub_449330
        case 0x1B: if (op) c.stats[ST_FORUM_CAP / 4] += prodCap(r); break;    // sub_448EF0
        case 0x22: if (op) c.stats[ST_MED_CAP / 4] += prodCap(r); break;      // sub_449AB0
        case 0x1E: case 0x34: case 0x35:                                       // residências
            if (op) c.stats[ST_RES_CAP / 4] += prodCap(r);
            break;
        case 0x23: if (op) ++c.stats[ST_NURSERY / 4]; break;                  // sub_449BD0
        case 0x26: if (op) ++c.stats[ST_UNIVERSITY / 4]; break;               // sub_448E30
        case 0x33: if (op) ++c.stats[ST_OBSERVATORY / 4]; break;              // sub_4499C0
        case 0x2B: if (op) ++c.stats[ST_METEOR_DEF / 4]; break;               // sub_44A130
        case 0x20: if (op) ++c.stats[ST_TRADE / 4]; break;                    // sub_449440
        default: break;   // nullsub_38 (torres, labs, fábricas, garagem...) e DIRT
    }
}

// ---- Eventos de moral (somam em +0x38) -------------------------------------
static int colonists(const PlayerData& pd) { return pd.workers + pd.scientists + pd.kids; }

// sub_44F120: KID_DIES por criança + ADULT_DIES por adulto.
static void moraleDeaths(int p, int kids, int sci, int workers) {
    if (kids + sci + workers == 0) return;
    g_colony[p].moraleEvents += kids * s_me[ME_KID_DIES] + (sci + workers) * s_me[ME_ADULT_DIES];
}
// sub_44F410: KID_BORN por criança (com pop >= 125, só +1 cada).
static void moraleBirths(int p, int n) {
    g_colony[p].moraleEvents += colonists(g_playerData[p]) < 0x7D ? n * s_me[ME_KID_BORN] : n;
}
// sub_44F520: TECH_SCHOOL por trabalhador formado (com pop >= 125, +1 cada).
static void moraleGrewUp(int p, int n) {
    g_colony[p].moraleEvents += colonists(g_playerData[p]) < 0x7D ? n * s_me[ME_TECH_SCHOOL] : n;
}
static bool isGoodBuilding(int t) {   // Nursery, Medical, residências
    return t == 0x23 || t == 0x22 || t == 0x1E || t == 0x35 || t == 0x34;
}
void moraleStructureLost(int player, int type) {             // sub_44F6C0
    if (player < 0 || player >= kMaxPlayers) return;
    g_colony[player].moraleEvents += s_me[isGoodBuilding(type) ? ME_GOOD_BUILD_DIES : ME_REG_BUILD_DIES];
}
void moraleEnemyStructureKilled(int player, int type) {      // sub_44FBE0
    if (player < 0 || player >= kMaxPlayers) return;
    if (isGoodBuilding(type)) g_colony[player].moraleEvents += s_me[ME_ENEMY_GOOD_DIES];
    else if (type == 0x32) g_colony[player].moraleEvents += s_me[ME_ENEMY_BAD_DIES];
}
void moraleStructureBuilt(int player, int type) {            // sub_44F790
    if (player < 0 || player >= kMaxPlayers) return;
    if (type == mapCommandCenter) g_colony[player].moraleEvents += s_me[ME_CC_BORN];
}

// ---- sub_471BD0: mata `n` colonos em rodízio ([+0x1EC]: 0,4 criança;
// 1,2 cientista; 3 trabalhador). quiet=true não avisa a moral.
static void killColonists(int p, int n, bool quiet) {
    ColonyState& c = g_colony[p];
    PlayerData& pd = g_playerData[p];
    int total = pd.workers + pd.kids + pd.scientists;
    if (n > total) n = total;
    int dk = 0, dw = 0, ds = 0;
    while (n) {
        const int rot = c.starveRotor;
        if (rot == 0 || rot == 4) { if (dk < pd.kids) { ++dk; --n; } }
        else if (rot == 1 || rot == 2) { if (ds < pd.scientists) { ++ds; --n; } }
        else if (rot == 3) { if (dw < pd.workers) { ++dw; --n; } }
        c.starveRotor = (rot + 1) % 5;
    }
    pd.kids -= dk; pd.workers -= dw; pd.scientists -= ds;
    if (!quiet) moraleDeaths(p, dk, ds, dw);
}

// ---- sub_471CC0: população ----------------------------------------------------
static void populationTick(int p) {
    ColonyState& c = g_colony[p];
    PlayerData& pd = g_playerData[p];
    const MoraleRow& m = moraleRow(pd.morale);
    // Crianças viram trabalhadores (exige University operando).
    if (pd.kids > 0 && c.stat(ST_UNIVERSITY)) {
        c.accGrowUp += pd.kids;
        int pop = pd.workers + pd.scientists + pd.kids;
        if (pop <= 0xA0) pop = 0xA0;
        const int denom = ((pop / 40) * 3 + 0x24) * 4;
        const int n = c.accGrowUp / denom;
        c.accGrowUp %= denom;
        if (n) { moraleGrewUp(p, n); pd.workers += n; pd.kids -= n; }
    }
    // Nascimentos (exige Nursery operando e adultos > crianças).
    if (c.stat(ST_NURSERY) && pd.scientists + pd.workers > pd.kids && m.fertRate) {
        c.accBirth += (pd.scientists >> 2) + (pd.workers >> 1);
        const int n = c.accBirth / m.fertRate;
        c.accBirth %= m.fertRate;
        if (n) { moraleBirths(p, n); pd.kids += n; }
    }
    int deadKids = 0, deadWorkers = 0, deadSci = 0;
    // Mortes de crianças: DIE/2 + Nurseries*40 + capacidade dos Medical Centers.
    if (pd.kids > 0) {
        c.accKidDeath += pd.kids;
        const int denom = (m.dieRate >> 1) + c.stat(ST_NURSERY) * 40 + c.stat(ST_MED_CAP);
        if (denom > 0) {
            deadKids = c.accKidDeath / denom;
            c.accKidDeath %= denom;
            pd.kids -= deadKids;
            if (pd.kids == 0) { c.accKidDeath = 0; c.accGrowUp = 0; }
        }
    }
    const int denomAdult = m.dieRate + c.stat(ST_MED_CAP);
    if (pd.workers && denomAdult > 0) {
        c.accWorkerDeath += pd.workers;
        deadWorkers = c.accWorkerDeath / denomAdult;
        c.accWorkerDeath %= denomAdult;
        pd.workers -= deadWorkers;
        if (pd.workers == 0) c.accWorkerDeath = 0;
    }
    if (pd.scientists && denomAdult > 0) {
        c.accSciDeath += pd.scientists;
        deadSci = c.accSciDeath / denomAdult;
        c.accSciDeath %= denomAdult;
        pd.scientists -= deadSci;
        if (pd.scientists == 0) c.accSciDeath = 0;
    }
    if (deadKids || deadSci || deadWorkers) moraleDeaths(p, deadKids, deadSci, deadWorkers);
}

// ---- Trecho de comida de sub_471F30 (slot de 32 ticks do jogador) -------------
static void foodTick(int p) {
    ColonyState& c = g_colony[p];
    PlayerData& pd = g_playerData[p];
    populationTick(p);
    c.foodProduced = 0;
    for (int id : s_structList[p]) {
        const UnitRecord& r = g_units[id];
        int e = 0;
        if (r.type == 0x27) {                                   // vt+0xD8 sub_483B30
            const int cap = prodCap(r);
            e = cap + ((moraleRow(pd.morale).foodBonus * cap + 0x7F) >> 8);
        }
        if (structureOperational(r)) c.foodProduced += e;
    }
    const int prevStored = pd.foodStored;
    const int avail = pd.foodStored + c.foodProduced;
    pd.foodStored = avail;
    const int pop = pd.kids + pd.workers + pd.scientists;
    const int consumption = ((pop > 9) ? 9 : 10) * pop / 10;
    if (consumption > avail) {
        c.foodEaten = avail;
        c.foodDeficit = consumption - avail;
        // "COLONISTS ARE STARVING TO DEATH" (jogador local) — mensagens: etapa B3.
        if (pop < 4) killColonists(p, 1, false);
        else {
            const int d = consumption - c.foodEaten;
            killColonists(p, (d - (d >> 31)) >> 1, false);
        }
    } else {
        c.foodEaten = avail < pop ? avail : pop;
        c.foodDeficit = 0;
        // "Food Supply is Diminishing/Growing", "Food Stores are Plentiful" — B3.
    }
    pd.foodStored -= c.foodEaten;
    c.foodNet = pd.foodStored - prevStored;
}

// ---- sub_471F30: alocação ----------------------------------------------------
static void allocate(int p) {
    ColonyState& c = g_colony[p];
    PlayerData& pd = g_playerData[p];
    std::vector<int>& L = s_structList[p];

    // [ebx+0xEC] = sub_490800(0x59): nº de satélites solares do jogador.
    c.solarSatellites = 0;
    for (size_t i = 1; i < g_units.size(); ++i)
        if (g_units[i].id != -1 && g_units[i].type == 0x59 && ownerOf(g_units[i]) == p)
            ++c.solarSatellites;

    // Energia dos geradores: ativos (vt+0xA4) somam ao disponível; os parados
    // vão p/ powerIdle.
    int powerAvail = 0, idle = 0;
    for (int id : L) {
        UnitRecord& r = g_units[id];
        int e = powerProduced(r);
        if (producing(r, c)) powerAvail += e; else idle += e;
    }
    c.powerGenerated = powerAvail;
    c.powerIdle = idle;
    // (dword_56DE58 — atalho que soma 0x100000 e dispensa tubos — fica em 0.)
    c.inactiveCount = c.powerRequired = c.workersRequired = 0;
    c.scientistsRequired = c.scientistsAsWorkers = c.researchAssigned = 0;
    c.unpoweredCount = c.activeCount = c.structCount = 0;
    int workers = pd.workers, scientists = pd.scientists;

    std::memset(c.stats, 0, sizeof(c.stats));               // [ebx+0xF0], 0x32 dwords
    std::vector<int> comp;
    computeComponents(p, comp);
    std::vector<uint8_t> compConnected(L.size() + 1, 0);  // sub_42FF60: tudo desligado
    c.components.assign(L.size(), ColonyState::NetComponent{});
    for (int id : L) if (comp[id] >= 0) ++c.components[comp[id]].structures;

    for (int id : L) {
        UnitRecord& r = g_units[id];
        const int powReq = powerRequired(r);
        const int wrkReq = workersRequired(r);
        const int sciReq = scientistsRequired(r);
        ++c.structCount;
        const bool inactive = (r.action == 0x12 || r.action == ACT_BUILD || r.action == ACT_DISMANTLE);
        if (inactive) {
            ++c.inactiveCount;                              // loc_4724E7
        } else {
            c.powerRequired += powReq;
            c.workersRequired += wrkReq;
            c.scientistsRequired += sciReq;
            const int cid = comp[id];
            bool connected = (cid >= 0 && compConnected[cid]) || r.type == mapCommandCenter;

            // Energia (não depende de tubo).
            if (powReq <= powerAvail && !(r.flags & 0x80000)) {
                if (!(r.flags & UNIT_POWERED)) r.flags |= UNIT_LIGHTS | UNIT_POWERED;  // vt+0xF0
                powerAvail -= powReq;
            } else {
                if (r.flags & UNIT_POWERED) r.flags &= ~(UNIT_POWERED | UNIT_LIGHTS);  // vt+0xBC
                ++c.unpoweredCount;
            }
            // Trabalhadores (cientistas cobrem a falta).
            if (wrkReq) {
                if (workers + scientists >= wrkReq && connected && !(r.flags & 0x40000)) {
                    r.flags |= UNIT_WORKERS;                                // vt+0xF4
                    workers -= wrkReq;
                    if (workers < 0) {
                        scientists += workers;
                        c.scientistsAsWorkers -= workers;
                        workers = 0;
                    }
                } else if (r.type == kUniversity && workers != 0) {
                    if (r.extraWorkers > workers) r.extraWorkers = workers;
                    r.flags |= UNIT_WORKERS;
                    workers -= workersRequired(r);
                } else if (r.flags & UNIT_WORKERS) {
                    r.flags &= ~UNIT_WORKERS;                               // vt+0xA8
                }
            } else {
                r.flags |= UNIT_WORKERS;
            }
            // Cientistas.
            if (sciReq) {
                if (sciReq <= scientists && connected && !(r.flags & 0x40000)) {
                    r.flags |= UNIT_SCIENTISTS;                             // vt+0xF8
                    scientists -= sciReq;
                    c.researchAssigned += researchAssigned(r);              // vt+0xE4
                } else if (r.action == 0x16 && scientists != 0) {         // pesquisando
                    if (r.extraScientists > static_cast<int8_t>(scientists))
                        r.extraScientists = static_cast<int8_t>(scientists);
                    r.flags |= UNIT_SCIENTISTS;
                    scientists -= scientistsRequired(r);
                    c.researchAssigned += researchAssigned(r);
                } else if (r.flags & UNIT_SCIENTISTS) {
                    r.flags &= ~UNIT_SCIENTISTS;                            // vt+0xB0
                }
            } else {
                r.flags |= UNIT_SCIENTISTS;
            }
        }
        statsTick(c, r, inactive);                          // vt+0x9C
        if (comp[id] >= 0) {
            ColonyState::NetComponent& k = c.components[comp[id]];
            if (r.type == mapCommandCenter) {               // sub_40C140: nó
                ++k.ccs;
                if (structureOperational(r)) { compConnected[comp[id]] = 1; }
            }
            if (r.type == 0x28 && structureOperational(r) && compConnected[comp[id]])
                ++k.dirts;                                  // sub_411F40
            k.connected = compConnected[comp[id]];
        }
        if (r.type == mapCommandCenter && structureOperational(r)) {
            const int e = powerProduced(r);
            c.powerGenerated += e;
            powerAvail += e;
            c.powerIdle -= e;
            if (comp[id] >= 0) compConnected[comp[id]] = 1;
        }
        if (structureOperational(r)) { ++c.activeCount; r.flags |= UNIT_LIGHTS; }
        else r.flags &= ~UNIT_LIGHTS;
    }
    // (Mensagens "%i Structures Disabled" e "Power Levels: Shortage/Marginal/
    //  Optimal" p/ o jogador local — Fase B.)
    c.powerLeft = powerAvail;
    c.workersLeft = workers;
    c.scientistsLeft = scientists;
    c.dirty = 0;
    // Capacidade de armazenamento (Smelters operando ou parados + Storages
    // operando) e corte do estoque. (dword_56DE58 → 100000; fica em 0.)
    auto storage = [&](int type) {
        const StructSheetDesc* d = structSheet(type);
        return d ? d->player[p].storageCapacity : 0;
    };
    c.oreCapacity = (c.stat(ST_SMELT_GRP + 4) + c.stat(ST_SMELT_GRP + 8)) * storage(0x36)
                  + storage(0x19) * c.stat(ST_STORE_GRP + 4);
    c.rareCapacity = (c.stat(ST_RSMELT_GRP + 4) + c.stat(ST_RSMELT_GRP + 8)) * storage(0x38)
                   + storage(0x1A) * c.stat(ST_RSTORE_GRP + 4);
    if (pd.ore > c.oreCapacity) pd.ore = c.oreCapacity;
    if (pd.rareOre > c.rareCapacity) pd.rareOre = c.rareCapacity;
    // ("Common/Rare Metals Storage Needed" — B3; moral sub_44E550 — B2.)
}

// ---- sub_42FE40: cobertura de DIRT (média dos componentes com CC) -----------
static int dirtCoverage(int p) {
    const ColonyState& c = g_colony[p];
    const StructSheetDesc* dirt = structSheet(0x28);
    int sum = 0, n = 0;
    for (const auto& k : c.components) {
        if (!k.ccs) continue;
        ++n;
        if (!k.dirts) continue;
        const int cap = dirt ? dirt->player[p].productionCapacity : 0;
        if (cap == 0) continue;
        const int ratio = ((k.structures - k.dirts) / k.dirts) * 100 / cap;
        sum += ratio <= 100 ? 0x9C4 : ratio <= 200 ? 0x6EF : ratio <= 300 ? 0x4E2 : 0x271;
    }
    return n ? sum / n : 0;
}

// ---- sub_44E550: moral (slot de 32 ticks do jogador) --------------------------
static void moraleTick(int p) {
    const int tick = TethysGame::tick();
    const int slot = (tick & 0x1F) ? 7 : ((tick / 32) & 7);
    if (slot != p) return;
    ColonyState& c = g_colony[p];
    PlayerData& pd = g_playerData[p];
    if (!pd.isHuman) {                      // IA: zera +0x34..+0x7B e nível 2
        c.moraleEvents = c.moraleValue = c.moraleTrend = 0;
        c.crowding = c.foodState = c.disabledPct = c.recreationLoad = c.medicalLoad = 0;
        c.hasNursery = c.hasUniversity = c.disasterTechs = c.dirtCoverage = 0;
        c.idlePct = c.phdPct = c.hasGorf = 0;
        pd.morale = 2;
        return;
    }
    const int prevValue = c.moraleValue;
    const int events = c.moraleEvents;
    c.moraleEvents = (s_me[ME_EVENT_DEC_RATE] * events + ((events * s_me[ME_EVENT_DEC_RATE]) >> 31 & 0xFF)) >> 8;
    const int pop = pd.kids + pd.workers + pd.scientists;
    if (pop == 0) return;
    // Condições.
    const int resCap = c.stat(ST_RES_CAP);
    c.crowding = resCap ? pop * 100 / resCap : 10000;
    if (c.foodEaten < c.foodProduced) c.foodState = 0;
    else if (c.foodDeficit) c.foodState = 3;
    else c.foodState = (pop * 15 > pd.foodStored) ? 2 : 1;
    if (c.activeCount) {
        c.disabledPct = (c.structCount - c.inactiveCount - c.activeCount) * 100 / c.structCount;
    } else {
        c.disabledPct = (c.structCount == c.inactiveCount) ? 0 : 10000;
    }
    const int recCap = c.stat(ST_REC_CAP) + c.stat(ST_FORUM_CAP);
    c.recreationLoad = recCap ? pop * 100 / recCap : 10000;
    const int medCap = c.stat(ST_MED_CAP);
    c.medicalLoad = medCap ? pop * 100 / medCap : 10000;
    c.hasNursery = c.stat(ST_NURSERY) ? 1 : 0;
    c.hasUniversity = c.stat(ST_UNIVERSITY) ? 1 : 0;
    c.hasGorf = c.stat(ST_GORF_GRP + 4) ? 1 : 0;
    c.dirtCoverage = dirtCoverage(p);
    c.idlePct = (pop == pd.kids) ? 0 : (c.workersLeft + c.scientistsLeft) * 100 / (pop - pd.kids);
    c.phdPct = pd.scientists ? c.scientistsAsWorkers * 100 / pd.scientists : 0;

    // Pontos por faixa.
    int pts = 0;
    {   int v = 0;                                            // lotação
        if (c.crowding <= 100) v = s_me[ME_CROWDED_NOT];
        if (c.crowding > 100) v += s_me[ME_CROWDED_LOW];
        if (c.crowding > 0x85) v += s_me[ME_CROWDED_MED];
        if (c.crowding > 0xA6) v += s_me[ME_CROWDED_HIGH];
        if (c.crowding > 0xC8) v += s_me[ME_CROWDED_MAX];
        pts += v; }
    pts += s_me[ME_FOOD_SURPLUS + c.foodState];               // comida
    {   int v = 0, d = c.disabledPct;                         // prédios desligados
        if (d == 0) v = s_me[ME_DIS_BLD_LOW];
        if (d > 0) v += s_me[ME_DIS_BLD_MED];
        if (d > 0x21) v += s_me[ME_DIS_BLD_HIGH];
        if (d > 0x42) v += s_me[ME_DIS_BLD_MAX];
        if (d == 10000) v = s_me[ME_DIS_BLD_LOW];
        pts += v; }
    auto loadPts = [&](int load, int base) {                  // REC_UT_* / MED_UT_*
        int v = 0;
        if (load <= 100) v = s_me[base];
        if (load <= 0x96) v += s_me[base + 1];
        if (load <= 0xC8) v += s_me[base + 2];
        else v += s_me[base + 3];
        if (load == 10000) v = 0;
        return v;
    };
    pts += loadPts(c.recreationLoad, ME_REC_UT_LOW);
    pts += loadPts(c.medicalLoad, ME_MED_UT_LOW);
    if (c.hasNursery) pts += s_me[ME_NURSERY_ON];
    if (c.hasUniversity) pts += s_me[ME_UNIV_ON];
    if (c.hasGorf) pts += 2;
    // Avisos de desastre (técnicas pesquisadas) — contam nos pontos E no teto.
    const _Player& pl = Player[p];
    int maxPts = s_meBaseMax, disPts = 0;
    c.disasterTechs = 0;
    static const struct { const char* name; int entry; } kDis[5] = {
        {"EARTHQUAKE", ME_DIS_QUAKE_ON}, {"LIGHTNING", ME_DIS_ELEC_ON},
        {"TORNADO", ME_DIS_SAND_ON}, {"ERUPTION", ME_DIS_VOL_ON}, {"METEOR", ME_DIS_MET_ON},
    };
    for (const auto& d : kDis)
        if (pl.HasTechnology(weaponResearchTopic(d.name))) { ++c.disasterTechs; disPts += s_me[d.entry]; }
    maxPts += disPts;
    pts += disPts;
    {   int v = 0, k = c.dirtCoverage;                        // DIRT
        if (k > 0) v = s_me[ME_DIRT_LOW];
        if (k >= 0x271) v += s_me[ME_DIRT_MED];
        if (k >= 0x4E2) v += s_me[ME_DIRT_HIGH];
        if (k >= 0x6EF) v += s_me[ME_DIRT_MAX];
        pts += v; }
    {   int v = 0, k = c.idlePct;                             // ociosos
        if (k >= 10) v = s_me[ME_UNOC_LOW];
        if (k >= 0x1E) v += s_me[ME_UNOC_MED];
        if (k >= 0x32) v += s_me[ME_UNOC_HIGH];
        pts += v; }
    {   int v = 0, k = c.phdPct;                              // cientistas na enxada
        if (k >= 5) v = s_me[ME_PHD_WHINE_LOW];
        if (k >= 0x14) v += s_me[ME_PHD_WHINE_MED];
        if (k >= 0x28) v += s_me[ME_PHD_WHINE_HIGH];
        if (k >= 0x41) v += s_me[ME_PHD_WHINE_MAX];
        pts += v; }
    // Teto possível: técnicas que liberam os prédios de bem-estar.
    auto topic = [](int type) { const StructSheetDesc* d = structSheet(type); return d ? d->researchTopic : -1; };
    if (pl.HasTechnology(topic(0x22))) maxPts += s_meMedSum;
    if (pl.HasTechnology(topic(0x23))) maxPts += s_me[ME_NURSERY_ON];
    if (pl.HasTechnology(topic(0x26))) maxPts += s_me[ME_UNIV_ON];
    if (pl.HasTechnology(topic(0x39))) maxPts += 2;
    if (pl.HasTechnology(topic(0x25))) maxPts += s_meRecSum;
    if (pl.HasTechnology(topic(0x28))) maxPts += s_meDirtSum;

    int score = events + (maxPts ? kMoraleScale * pts / maxPts : 0);
    const int diff = pd.difficulty < 0 ? 0 : pd.difficulty > 2 ? 2 : pd.difficulty;
    score = (score * s_me[(pd.isEden ? ME_DIFF_EASY : ME_DIFF_EASY_PLY) + diff]) >> 8;
    if (score > prevValue + 5) score = prevValue + 5;          // ±5 por ciclo
    else if (score < prevValue - 5) score = prevValue - 5;
    // (ForceMorale* das missões — bitmasks dword_56DEAC..BC: 99/75/50/25/0.)
    int level = 0;
    while (level < 4 && moraleRow(level).points > score) ++level;
    pd.morale = level;
    if (score < 0) score = 0;
    if (score > 0x63) score = 0x63;
    c.moraleValue = score;
    c.moraleTrend = score > prevValue ? 1 : score < prevValue ? -1 : 0;
    // ("Morale Level: %s" p/ o jogador local e o MORALE.LOG — mensagens/B3.)
}

static void colonyPlayerTick(int p) {
    ColonyState& c = g_colony[p];
    PlayerData& pd = g_playerData[p];
    const int tick = TethysGame::tick();
    const bool slot = ((tick / 32) & 7) == p;
    if (!pd.isHuman) {
        pd.workers = 0x1000; pd.scientists = 0x1000; pd.kids = 0x100;
        c.dirty = (tick & 0x1F) ? (p == 7) : slot;
    } else if ((tick & 0x1F) ? (p == 7) : slot) {
        foodTick(p);                                    // comida + população
        c.dirty = 1;
    }
    if (c.dirty) allocate(p);
    moraleTick(p);                                         // sub_44E550 (fim de sub_471F30)
}

void tickColony() {
    // sub_489DD0: p/ cada jogador (exceto humanos em certos tipos de missão,
    // [dword_4EFD24+0xC] — sem descritor de missão no port).
    for (int p = 0; p < TethysGame::noPlayers() && p < kMaxPlayers; ++p)
        colonyPlayerTick(p);
}

} // namespace op2
