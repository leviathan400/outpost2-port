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

    std::vector<int> comp;
    computeComponents(p, comp);
    std::vector<uint8_t> compConnected(L.size() + 1, 0);  // sub_42FF60: tudo desligado

    for (int id : L) {
        UnitRecord& r = g_units[id];
        const int powReq = powerRequired(r);
        const int wrkReq = workersRequired(r);
        const int sciReq = scientistsRequired(r);
        ++c.structCount;
        if (r.action == 0x12 || r.action == ACT_BUILD || r.action == ACT_DISMANTLE) {
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
        // (vt+0x9C: estatísticas por tipo — usadas por armazenamento/relatórios,
        //  Fase B.)
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
    // (Capacidade de armazenamento de minério e moral sub_44E550 — Fase B.)
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
        // Comida/população (sub_471CC0, produção vt+0xD8, consumo, fome
        // sub_471BD0) — Fase B. Ao fim do trecho o original força dirty=1.
        c.dirty = 1;
    }
    if (c.dirty) allocate(p);
}

void tickColony() {
    // sub_489DD0: p/ cada jogador (exceto humanos em certos tipos de missão,
    // [dword_4EFD24+0xC] — sem descritor de missão no port).
    for (int p = 0; p < TethysGame::noPlayers() && p < kMaxPlayers; ++p)
        colonyPlayerTick(p);
}

} // namespace op2
