#include "trigger.h"
#include "../audio/sound.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

#include "building_sheet.h"
#include "colony.h"
#include "game_map.h"
#include "player.h"
#include "scenario.h"
#include "tethys_game.h"
#include "unit.h"
#include "ai_group.h"
#include "../mission/mission.h"

namespace op2 {

// ---- Tabela de stubs (dword_56B71C[255], último usado dword_56B718) -----------
static const int kMaxStubs = 0xFF;

// FuncReference (off_4E3C58; sub_475710): nome copiado (+0x20), origem (+0x1C:
// 1 = script da missão) e endereço resolvido (+0x24).
struct FuncReference : Stub {
    std::string name;
    int fromScript = 0;
    void (*fn)() = nullptr;
};

enum TriggerKind {
    TK_TIME, TK_RESEARCH, TK_VICTORY, TK_FAILURE,
    TK_COUNT, TK_KIT, TK_BUILDING_COUNT, TK_VEHICLE_COUNT, TK_ESCAPE, TK_SET,
    TK_RESOURCE, TK_EVAC, TK_OPERATIONAL, TK_RECT, TK_SPECIAL,
    TK_DAMAGED, TK_POINT, TK_ONE_LEFT,
};

// _Trigger (off_4E9E50) e derivadas. Campos +0x30/+0x34 conforme a classe.
struct Trigger : Stub {
    TriggerKind kind = TK_TIME;
    int oneShot = 0;           // +0x1C
    uint32_t playerMask = 0;   // +0x20
    int funcRef = 0;           // +0x24 (stubIndex da FuncReference; 0 = nenhuma)
    int f30 = 0, f34 = 0;      // +0x30 / +0x34
    int f38 = 0, f3C = 0, f40 = 0, f44 = 0, f48 = 0, f4C = 0, f50 = 0;   // +0x38..+0x50
    std::vector<int> set;      // SetTrigger: +0x38.. stubs dos triggers do conjunto
    const char* text = nullptr;  // Victory/FailureCondition: +0x30 (texto da DLL)
};

static Stub* s_stubs[kMaxStubs] = {};
static int s_lastStub = 0;
// Listas de instâncias (inserção na cabeça): _Trigger (dword_4E9E64),
// VictoryCondition (dword_4EA0C4), FailureCondition (dword_4EA0E4).
static std::vector<int> s_triggers, s_victory, s_failure;
static std::vector<int> s_special;   // SpecialTargetTrigger (dword_4EA084, inserção na cabeça)

static int s_endCode = 0;      // TethysGame +0x9C (dword_56DEF4)
static int s_endTick = 0x7FFFFFFF;   // TethysGame +0x98
static int s_speed = 40;       // TethysGame +0x50

void setGameSpeedValue(int v) { s_speed = v; }
int gameEndCode() { return s_endCode; }
int gameEndTick() { return s_endTick; }

void resetTriggers() {
    for (Stub*& s : s_stubs) { delete s; s = nullptr; }
    s_lastStub = 0;
    s_triggers.clear(); s_victory.clear(); s_failure.clear();
    aiGroupsReset();
    s_special.clear();
    s_endCode = 0;
    s_endTick = 0x7FFFFFFF;
}

// sub_47B460: aloca o próximo índice livre depois do último usado.
static int allocStub(Stub* s) {
    for (int i = (s_lastStub + 1) % kMaxStubs; i != s_lastStub && i < kMaxStubs; ++i) {
        if (!s_stubs[i]) {
            s_stubs[i] = s;
            s_lastStub = i;
            s->id = i;
            return i;
        }
    }
    delete s;
    return 0xFF;
}

int stubAllocate(Stub* s) { return allocStub(s); }
Stub* stubObject(int idx) { return idx >= 0 && idx < kMaxStubs ? s_stubs[idx] : nullptr; }

static Trigger* trig(int stub) {
    if (stub < 0 || stub >= kMaxStubs) return nullptr;
    return dynamic_cast<Trigger*>(s_stubs[stub]);
}

// sub_475710 + sub_4034B0: resolve o nome da função no script da missão.
static int createFuncRef(const char* name) {
    auto* f = new FuncReference();
    f->name = name ? name : "";
    f->fromScript = 1;
    MissionScript* m = currentMission();
    f->fn = m ? reinterpret_cast<void (*)()>(m->proc(f->name)) : nullptr;
    if (!f->fn) {
        std::cerr << "[Trigger] Couldn't resolve data reference " << f->name
                  << " in mission script" << std::endl;
        delete f;
        return 0xFF;
    }
    return allocStub(f);
}

// vt[2] (sub_491FD0) + sub_491F00(bEnabled, bOneShot, funcRef).
static int newTrigger(TriggerKind kind, int bEnabled, int bOneShot, int funcRef, std::vector<int>* list) {
    auto* t = new Trigger();
    t->kind = kind;
    t->enabled = bEnabled;
    t->oneShot = bOneShot;
    t->funcRef = funcRef;
    t->playerMask = 0;
    const int id = allocStub(t);
    if (id == 0xFF) return 0xFF;
    s_triggers.insert(s_triggers.begin(), id);              // cabeça da lista _Trigger
    if (list) list->insert(list->begin(), id);
    return id;
}

int createTimeTrigger(int bEnabled, int bOneShot, int time, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_TIME, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) { t->f34 = time; t->f30 = time; }    // sub_4920A0
    return id;
}

int createResearchTrigger(int bEnabled, int bOneShot, int techID, int playerNum, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_RESEARCH, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_4926D0
        t->f34 = playerNum;
        t->f30 = techID;
        if (playerNum != -1) t->playerMask = (1u << playerNum) ^ 0x3Fu;
    }
    return id;
}

int createVictoryCondition(int bEnabled, int bOneShot, int triggerStub, const char* text) {
    const int id = newTrigger(TK_VICTORY, bEnabled, bOneShot, 0, &s_victory);
    if (Trigger* t = trig(id)) { t->f34 = triggerStub; t->text = text; }   // sub_4957A0
    return id;
}

int createFailureCondition(int bEnabled, int bOneShot, int triggerStub, const char* text) {
    const int id = newTrigger(TK_FAILURE, bEnabled, bOneShot, 0, &s_failure);
    if (Trigger* t = trig(id)) { t->f34 = triggerStub; t->text = text; }
    return id;
}

// Init comum: jogador >= 0 → [+0x20] = (1 << jogador) ^ 0x3F; senão 0.
static void initPlayerMask(Trigger& t, int player) {
    t.playerMask = player >= 0 ? (1u << player) ^ 0x3Fu : 0u;
}

// Os Create* abaixo: FuncReference, depois o trigger (sub_47B460 + sub_491F00)
// e o init da classe.
int createCountTrigger(int bEnabled, int bOneShot, int player, int type, int cargo, int count,
                       int compare, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_COUNT, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_493AA0
        t->f30 = type; t->f34 = cargo; t->f38 = count; t->f3C = player; t->f40 = compare;
        initPlayerMask(*t, player);
    }
    return id;
}

int createKitTrigger(int bEnabled, int bOneShot, int player, int type, int count, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_KIT, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_494240
        t->f30 = type; t->f34 = count; t->f38 = player;
        initPlayerMask(*t, player);
    }
    return id;
}

int createBuildingCountTrigger(int bEnabled, int bOneShot, int player, int count, int compare, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_BUILDING_COUNT, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_494B70
        t->f30 = count; t->f34 = player; t->f38 = compare;
        initPlayerMask(*t, player);
    }
    return id;
}

int createVehicleCountTrigger(int bEnabled, int bOneShot, int player, int count, int compare, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_VEHICLE_COUNT, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_494E30
        t->f30 = count; t->f34 = player; t->f38 = compare;
        initPlayerMask(*t, player);
    }
    return id;
}

int createEscapeTrigger(int bEnabled, int bOneShot, int player, int x, int y, int w, int h, int refValue,
                        int type, int cargoType, int cargoAmount, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_ESCAPE, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_493730 (tiles INTERNOS → px)
        t->f30 = x << 5; t->f34 = y << 5; t->f38 = w << 5; t->f3C = h << 5;
        t->f40 = refValue; t->f44 = type; t->f48 = cargoType; t->f4C = cargoAmount;
        if (player >= 0) {
            t->f50 = static_cast<int>(1u << player);
            t->playerMask = (1u << player) ^ 0x3Fu;
        } else {
            t->f50 = -1;
        }
    }
    return id;
}

int createRectTrigger(int bEnabled, int bOneShot, int player, int x, int y, int w, int h, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_RECT, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_492AC0
        t->f30 = x << 5; t->f34 = y << 5; t->f38 = w << 5; t->f3C = h << 5;
        if (player >= 0) {
            t->f40 = static_cast<int>(1u << player);
            t->playerMask = static_cast<uint32_t>(t->f40) ^ 0x3Fu;
        } else {
            t->f40 = -1;
        }
    }
    return id;
}

int createSpecialTargetTrigger(int bEnabled, int bOneShot, int targetUnit, int sourceType, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_SPECIAL, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_495130
        if (targetUnit > 0 && targetUnit < static_cast<int>(g_units.size()))
            g_units[targetUnit].flags |= 0x2000000u;
        t->f30 = targetUnit; t->f38 = sourceType; t->f34 = -1; t->f3C = 0;
        s_special.insert(s_special.begin(), id);
    }
    return id;
}
int createDamagedTrigger(int bEnabled, int bOneShot, int groupStub, int count, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_DAMAGED, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) { t->f30 = groupStub; t->f34 = count; }   // sub_4934F0
    return id;
}

int createPointTrigger(int bEnabled, int bOneShot, int player, int x, int y, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_POINT, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_492D80
        t->f30 = x << 5; t->f34 = y << 5;
        if (player >= 0) {
            t->f38 = static_cast<int>(1u << player);
            t->playerMask = static_cast<uint32_t>(t->f38) ^ 0x3Fu;
        } else {
            t->f38 = -1;
        }
    }
    return id;
}

int createOnePlayerLeftTrigger(int bEnabled, int bOneShot, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    return newTrigger(TK_ONE_LEFT, bEnabled, bOneShot, fr, nullptr);
}

// sub_490C80: o jogador p e todo j que tem p na sua máscara de aliança.
static uint32_t alliesOf(int p) {
    uint32_t m = 1u << p;
    for (int j = 0; j < TethysGame::noPlayers(); ++j)
        if (g_playerData[j].allyMask & (1u << p)) m |= 1u << j;
    return m;
}

int specialTargetSource(int stub) {
    const Trigger* t = trig(stub);
    return t && t->kind == TK_SPECIAL ? t->f34 : -1;
}

// Vizinha (4 direções) com alvo especial: id do alvo (0 = nenhum).
static int specialNeighbor(const UnitRecord& u) {
    const int x = u.pixelX >> 5, y = u.pixelY >> 5, mask = g_gameMap.width() - 1;
    const int cand[4][2] = {{(x - 1) & mask, y}, {(x + 1) & mask, y}, {x, y - 1}, {x, y + 1}};
    for (const auto& c : cand) {
        if (c[1] < 0 || c[1] >= g_gameMap.height()) continue;
        const int o = g_gameMap.cellOccupant(c[0], c[1]);
        if (o > 0 && o < static_cast<int>(g_units.size()) && (g_units[o].flags & 0x2000000u)) return o;
    }
    return 0;
}
bool specialTargetCheck(int unitId) {                                // sub_44D5B0
    UnitRecord& u = g_units[unitId];
    if (u.flags & 0x40u) return false;
    if (!specialNeighbor(u)) return false;
    u.action = 0x10; u.field20 = 1; u.field3C = 0;
    return true;
}
void specialTargetArrive(int unitId) {                               // sub_44D7D0 → sub_495170
    const UnitRecord& u = g_units[unitId];
    const int x = u.pixelX >> 5, y = u.pixelY >> 5, mask = g_gameMap.width() - 1;
    const int cand[4][2] = {{(x - 1) & mask, y}, {(x + 1) & mask, y}, {x, y - 1}, {x, y + 1}};
    for (const auto& c : cand) {
        if (c[1] < 0 || c[1] >= g_gameMap.height()) continue;
        const int o = g_gameMap.cellOccupant(c[0], c[1]);
        if (o <= 0 || o >= static_cast<int>(g_units.size()) || !(g_units[o].flags & 0x2000000u)) continue;
        for (int id : s_special) {
            Trigger* t = trig(id);
            if (!t || t->f30 != o || t->f38 != u.type) continue;
            t->f34 = unitId; t->f3C = 1;
            return;
        }
    }
}

int createSetTrigger(int bEnabled, int bOneShot, int total, int needed, const char* func, const int* stubs) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_SET, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_493020 / sub_493050
        t->f30 = total; t->f34 = needed;
        t->set.assign(total > 8 ? total : 8, -1);
        for (int i = 0; i < total; ++i) t->set[i] = stubs[i];
    }
    return id;
}

int createResourceTrigger(int bEnabled, int bOneShot, int resType, int amount, int player, int compare,
                          const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_RESOURCE, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_4922C0
        t->f30 = resType; t->f34 = amount; t->f38 = player; t->f3C = compare;
        if (player != -1) t->playerMask = (1u << player) ^ 0x3Fu;
    }
    return id;
}

int createEvacTrigger(int bEnabled, int bOneShot, int player, const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_EVAC, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) { t->f30 = player; initPlayerMask(*t, player); }   // sub_4945D0
    return id;
}

int createOperationalTrigger(int bEnabled, int bOneShot, int player, int type, int count, int compare,
                             const char* func) {
    const int fr = createFuncRef(func);
    if (fr == 0xFF) return 0xFF;
    const int id = newTrigger(TK_OPERATIONAL, bEnabled, bOneShot, fr, nullptr);
    if (Trigger* t = trig(id)) {                                     // sub_4953B0
        t->f30 = type; t->f34 = count; t->f38 = player; t->f3C = compare;
        initPlayerMask(*t, player);
    }
    return id;
}

bool triggerHasFired(int stub, int player) {                // ?HasFired (bit de [+0x20])
    const Trigger* t = trig(stub);
    return t && player >= 0 && player < 32 && (t->playerMask & (1u << player));
}

void stubEnable(int stub, bool on) {                       // vt[3]/vt[4]
    if (stub >= 0 && stub < kMaxStubs && s_stubs[stub]) s_stubs[stub]->enabled = on ? 1 : 0;
}
bool stubEnabled(int stub) {
    return stub >= 0 && stub < kMaxStubs && s_stubs[stub] && s_stubs[stub]->enabled;
}
void stubDestroy(int stub) {
    if (stub < 0 || stub >= kMaxStubs || !s_stubs[stub]) return;
    for (std::vector<int>* L : {&s_triggers, &s_victory, &s_failure, &s_special}) {
        auto it = std::find(L->begin(), L->end(), stub);
        if (it != L->end()) L->erase(it);
    }
    delete s_stubs[stub];
    s_stubs[stub] = nullptr;
}

// compare_mode: 0 igual, 1 <=, 2 >=, 3 <, 4 > (contagem comparada ao alvo).
static bool compareCount(int mode, int target, int n) {
    switch (mode) {
        case 0: return target == n;
        case 1: return target >= n;
        case 2: return target <= n;
        case 3: return target > n;
        case 4: return target < n;
        default: return false;
    }
}

// sub_493AF0, jogador específico: prédios pelo tipo (PlayerBuildingEnum),
// unidades espaciais (+0x08 do jogador) ou veículos (PlayerUnitEnum) com o filtro
// de carga. As garagens (0x29) também contam os veículos das baias — garagem
// ainda não portada (baias sempre vazias).
static int countForPlayer(int p, int type, int cargo) {
    int n = 0;
    if (type >= 0x15 && type <= 0x3A) {
        for (int b : colonyStructures(p)) if (g_units[b].type == type) ++n;
        return n;
    }
    if (type >= 0x58 && type <= 0x6B) {
        if (type == 0x58 || type == 0x59) return playerSpaceCount(p, type);
        if (type == 0x69 || type == 0x6B) return 0;
        if (type == 0x6A) {
            if (playerSpaceCount(p, 0x6A)) return 1;
            for (int b : colonyStructures(p))                       // RLV na plataforma (0x37, +0x6E)
                if (g_units[b].type == 0x37 && g_units[b].padCargo == 0x6A) return 1;
            return 0;
        }
        return playerSpaceHas(p, type);
    }
    for (int v : colonyVehicles(p)) {
        const UnitRecord& r = g_units[v];
        if (r.type != type) continue;
        if (cargo == -1) { ++n; continue; }
        if (type == 1) {
            if ((r.cargo & 0xF) == cargo) ++n;
        } else if (type == 2 || (type >= 5 && type <= 7) || type == 0x0D) {
            if (static_cast<int16_t>(r.cargo) == cargo) ++n;
        }
    }
    return n;
}

// sub_493AF0, todos os jogadores: a contagem NÃO zera entre jogadores (soma),
// salvo as espaciais, que sobrescrevem; sem garagens, e o filtro de carga só
// vale p/ os tipos 1, 2 e 0x0D.
static int countAccumulate(int p, int type, int cargo, int acc) {
    if (type >= 0x15 && type <= 0x3A) {
        for (int b : colonyStructures(p)) if (g_units[b].type == type) ++acc;
        return acc;
    }
    if (type >= 0x58 && type <= 0x6B) {
        if (type == 0x58 || type == 0x59 || type == 0x6A) return playerSpaceCount(p, type);
        if (type == 0x69 || type == 0x6B) return acc;
        return playerSpaceHas(p, type);
    }
    for (int v : colonyVehicles(p)) {
        const UnitRecord& r = g_units[v];
        if (r.type != type) continue;
        if (cargo == -1) { ++acc; continue; }
        if (type == 1) {
            if ((r.cargo & 0xF) == cargo) ++acc;
        } else if (type == 2 || type == 0x0D) {
            if (static_cast<int16_t>(r.cargo) == cargo) ++acc;
        }
    }
    return acc;
}

// trig_res: 0 comida (+0x10), 1 minério comum (+0x20), 2 raro (+0x24),
// 3 crianças (+0x9C), 4 trabalhadores (+0x94), 5 cientistas (+0x98),
// 6 colonos (3+4+5), 7 comum + raro.
static int resourceOf(int p, int res, int fallback) {
    const PlayerData& d = g_playerData[p];
    switch (res) {
        case 0: return d.foodStored;
        case 1: return d.ore;
        case 2: return d.rareOre;
        case 3: return d.kids;
        case 4: return d.workers;
        case 5: return d.scientists;
        case 6: return d.kids + d.workers + d.scientists;
        case 7: return d.rareOre + d.ore;
        default: return fallback;                             // (registrador sem valor novo)
    }
}

// vt[9]: disparou?
static bool hasFired(Trigger& t) {
    const int n = TethysGame::noPlayers();
    switch (t.kind) {
        case TK_TIME:                                         // sub_4920B0
            t.f34 -= 4;
            if (t.f34 > 0) return false;
            t.playerMask = 0x3F;
            t.f34 += t.f30;
            return true;
        case TK_RESEARCH: {                                   // sub_492700
            // sub_472C50: o jogador tem a tecnologia (ou a trapaça "produce all",
            // dword_56DE5C). A árvore de pesquisa completa (dword_56B5F0) ainda
            // não está portada; vale o conjunto de tecnologias do jogador.
            auto has = [&](int p) {
                return TethysGame::cheatFlag(1) != 0 || Player[p].HasTechnology(t.f30) != 0;
            };
            if (t.f34 == -1) {
                for (int p = 0; p < n; ++p) if (has(p)) t.playerMask |= 1u << p;
            } else if (has(t.f34)) {
                t.playerMask |= 1u << t.f34;
            }
            return t.playerMask != 0;
        }
        case TK_COUNT: {                                      // sub_493AF0
            t.playerMask = 0;
            if (t.f3C > -1) {
                if (!compareCount(t.f40, t.f38, countForPlayer(t.f3C, t.f30, t.f34))) return false;
                t.playerMask = 0x3F;
                return true;
            }
            int acc = 0;
            for (int p = 0; p < n; ++p) {
                acc = countAccumulate(p, t.f30, t.f34, acc);
                if (compareCount(t.f40, t.f38, acc)) t.playerMask |= 1u << p;
            }
            return t.playerMask != 0;
        }
        case TK_KIT: {                                        // sub_494280
            t.playerMask = 0;
            if (t.f38 < 0 || t.f38 >= kMaxPlayers) return false;
            int kits = 0;
            for (int b : colonyStructures(t.f38)) {
                const UnitRecord& r = g_units[b];
                // Garagem (0x29): ConVecs com o kit nas baias — garagem não portada.
                if (r.type == 0x2F) {                         // fábrica: vt[65] = tipo da baia
                    const StructSheetDesc* d = structSheet(r.type);
                    const int bays = d ? d->player[(r.owner >> 4) & 0x0F].storageBays : 0;
                    for (int i = 0; i < bays && i < 6; ++i)
                        if (static_cast<int8_t>(r.bayType[i]) == t.f30) ++kits;
                }
            }
            for (int v : colonyVehicles(t.f38))
                if (g_units[v].type == 2 && static_cast<int16_t>(g_units[v].cargo) == t.f30) ++kits;
            if (t.f34 > kits) return false;
            t.playerMask = 0x3F;
            return true;
        }
        case TK_BUILDING_COUNT:                               // sub_494BB0 (+0xC4 do jogador)
            t.playerMask = 0;
            if (t.f34 < 0 || t.f34 >= kMaxPlayers) return false;
            if (!compareCount(t.f38, t.f30, g_colony[t.f34].structCount)) return false;
            t.playerMask = 0x3F;
            return true;
        case TK_VEHICLE_COUNT: {                              // sub_494E70
            t.playerMask = 0;
            if (t.f34 < 0 || t.f34 >= kMaxPlayers) return false;
            // Lista de veículos + vt[63] das garagens (veículos guardados; não portado).
            const int cnt = static_cast<int>(colonyVehicles(t.f34).size());
            if (!compareCount(t.f38, t.f30, cnt)) return false;
            t.playerMask = 0x3F;
            return true;
        }
        case TK_RECT: {                                       // sub_492B10
            std::vector<int> found;
            mapUnitsInRect(t.f30, t.f34, t.f38, t.f3C, static_cast<uint32_t>(t.f40), 0xFFFFFFFFu, found);
            if (found.empty()) return false;
            if (t.f40 == -1) t.playerMask = 0x3Fu; else t.playerMask |= static_cast<uint32_t>(t.f40);
            return true;
        }
        case TK_SPECIAL:                                      // sub_495150
            if (!t.f3C) return false;
            t.f3C = 0;
            t.playerMask = 0x3Fu;
            return true;
        case TK_ESCAPE: {                                     // sub_4937A0
            t.playerMask = 0;
            std::vector<int> found;
            mapUnitsInRect(t.f30, t.f34, t.f38, t.f3C, static_cast<uint32_t>(t.f50), 0xFFFFFFFFu, found);
            int units = 0, cargo = 0;
            for (int id : found) {
                const UnitRecord& r = g_units[id];
                if (r.type != t.f44) continue;
                if (t.f48 == -1) { ++units; continue; }
                if (r.type == 1) {
                    if ((r.cargo & 0xF) == t.f48) { ++units; cargo += (r.cargo >> 4) & 0xFFF; }
                } else if (r.type == 2 || r.type == 0x0D) {
                    if (static_cast<int16_t>(r.cargo) == t.f48) { ++units; ++cargo; }
                }
            }
            if (t.f40 > units || t.f4C > cargo) return false;
            t.playerMask = t.f50 == -1 ? 0x3Fu : static_cast<uint32_t>(t.f50);
            return true;
        }
        case TK_SET:                                          // sub_493060 ([+0x20] acumula)
            for (int p = 0; p < n; ++p) {
                int hits = 0;
                for (int i = 0; i < t.f30 && i < static_cast<int>(t.set.size()); ++i) {
                    const Trigger* s = trig(t.set[i]);
                    if (s && (s->playerMask & (1u << p))) ++hits;
                }
                if (t.f34 <= hits) t.playerMask |= 1u << p;
            }
            return t.playerMask != 0;
        case TK_RESOURCE: {                                   // sub_492300
            t.playerMask = 0;
            if (t.f38 == -1) {                                // todos: sempre ">=" (ignora o modo)
                int v = 0;
                for (int p = 0; p < n; ++p) {
                    v = resourceOf(p, t.f30, v);
                    if (v >= t.f34) t.playerMask |= 1u << p;
                }
                return t.playerMask != 0;
            }
            if (t.f38 < 0 || t.f38 >= kMaxPlayers) return false;
            if (!compareCount(t.f3C, t.f34, resourceOf(t.f38, t.f30, 0))) return false;
            t.playerMask = 1u << t.f38;                       // só o bit do jogador
            return true;
        }
        case TK_EVAC: {                                       // sub_494600
            t.playerMask = 0;
            if (t.f30 < 0 || t.f30 >= kMaxPlayers) return false;
            const PlayerData& d = g_playerData[t.f30];
            const int colonists = d.workers + d.scientists + d.kids;
            int transports = 0;                               // + baias das garagens (não portada)
            for (int v : colonyVehicles(t.f30)) if (g_units[v].type == 0x0D) ++transports;
            const uint8_t* blk = unitStatBlock(0x0D, t.f30);  // capacidade (bloco +0x38)
            int cap = 0;
            if (blk) std::memcpy(&cap, blk + 0x38, 4);
            if (cap * transports < colonists) return false;
            t.playerMask = 0x3F;
            return true;
        }
        case TK_OPERATIONAL: {                                // sub_4953F0
            t.playerMask = 0;
            auto count = [&](int p) {
                int c = 0;
                if (p < 0 || p >= kMaxPlayers) return 0;      // (lista do "jogador −1": vazia)
                for (int b : colonyStructures(p))
                    if (g_units[b].type == t.f30 && structureOperational(g_units[b])) ++c;
                return c;
            };
            if (t.f38 > -1) {
                if (!compareCount(t.f3C, t.f34, count(t.f38))) return false;
                t.playerMask = 0x3F;
                return true;
            }
            // Jogador −1: o original relê sempre a lista de [+0x38] (= −1) e para
            // no primeiro jogador cuja comparação passa.
            for (int p = 0; p < n; ++p)
                if (compareCount(t.f3C, t.f34, count(t.f38))) { t.playerMask |= 1u << p; return true; }
            return false;
        }
        case TK_DAMAGED:                                      // sub_493510
            if (!stubObject(t.f30)) return false;
            if (aiUnitCount(t.f30, 0x11) >= t.f34) return false;
            t.playerMask = 0x3F;
            return true;
        case TK_POINT: {                                      // sub_492DC0
            MapPointIter it;
            mapPointIterInit(it, t.f30, t.f34, static_cast<uint32_t>(t.f38), 0xFFFFFFFFu);
            if (!mapPointIterNext(it)) return false;
            if (t.f38 == -1) t.playerMask = 0x3Fu; else t.playerMask |= static_cast<uint32_t>(t.f38);
            return true;
        }
        case TK_ONE_LEFT: {                                   // sub_492970
            uint32_t alive = 0, allied = 0xFFFFFFFFu;
            for (int p = 0; p < n; ++p) {
                if (!g_playerData[p].isHuman || !g_colony[p].stat(ST_CC_OP)) continue;   // +0x28 / +0x100
                alive |= 1u << p;
                allied &= alliesOf(p);
            }
            if ((alive & allied) != alive) return false;
            t.playerMask = allied;
            return true;
        }
        case TK_VICTORY:                                      // sub_4957C0
        case TK_FAILURE: {                                    // sub_495A10
            t.playerMask = 0;
            Trigger* ref = trig(t.f34);
            if (ref)
                for (int p = 0; p < n; ++p)
                    if (ref->playerMask & (1u << p)) t.playerMask |= 1u << p;
            return t.playerMask != 0;
        }
    }
    return false;
}

// vt[10] (sub_491DA0): oneShot → desabilita; devolve a função da FuncReference.
static void (*triggerCallback(Trigger& t))() {
    if (t.oneShot) t.enabled = 0;
    if (!t.funcRef || t.funcRef >= kMaxStubs) return nullptr;
    auto* f = dynamic_cast<FuncReference*>(s_stubs[t.funcRef]);
    return f ? f->fn : nullptr;
}

const char* gameEndText(int code) {                         // sub_49CAC0
    switch (code) {
        case 3: return "MISSION ACCOMPLISHED";
        case 4: return "MISSION FAILED";
        case 6: return "MISSION ABORTED";
        case 7: return "VICTORY IS SWEET";
        case 8: return "YOU HAVE BEEN DEFEATED";
        case 9: return "CONTROL TERMINATED";
        case 10: case 11: return "OUTPOST 2 DEMO";
        default: return "GAME OVER";
    }
}

// sub_48A650(código, atraso): só a primeira vez; encerra em atraso*velocidade
// ticks (2 se o atraso é 0).
static void endGame(int code, int delay) {
    if (s_endCode) return;
    s_endCode = code;
    s_endTick = TethysGame::tick() + (delay ? s_speed * delay : 2);
    std::cout << "[Mission] fim de jogo: " << gameEndText(code) << std::endl;
}

static bool isMultiplayerType(MissionScript* m) {           // tipo em [-8, -4]
    return m && m->desc && m->desc->missionType <= -4 && m->desc->missionType >= -8;
}

void missionTriggerTick(MissionScript* mission) {          // sub_403110
    if ((TethysGame::tick() & 3) != 3) return;
    // 1) Triggers: coleta as funções dos que dispararam e chama depois.
    std::vector<void (*)()> fire;
    for (int id : std::vector<int>(s_triggers)) {
        Trigger* t = trig(id);
        if (!t || !t->enabled) continue;
        if (!hasFired(*t)) continue;
        if (auto fn = triggerCallback(*t)) fire.push_back(fn);
    }
    for (auto fn : fire) fn();
    // 2) "Strategy" (dword_4E4194, vt[9]) — não portado; grupos (dword_4E0B64):
    //    vt[12], DeleteWhenEmpty e vt[13].
    aiGroupsTick();
    // 3) AIProc da missão.
    if (mission && mission->aiProc) mission->aiProc();
    // 4) Vitória: todas as VictoryConditions com o bit do jogador humano.
    const int n = TethysGame::noPlayers();
    uint32_t humans = 0;
    for (int p = 0; p < n; ++p) if (g_playerData[p].isHuman) humans |= 1u << p;
    uint32_t win = humans;
    const int local = TethysGame::localPlayer();
    // Sem nenhuma VictoryCondition (sub_47B580 == 0) o teste de vitória é pulado.
    if (!s_victory.empty()) {
        for (int id : s_victory) {
            Trigger* t = trig(id);
            win &= t ? t->playerMask : 0;
            if (!win) break;
        }
        // Multiplayer (tipo -8..-4): sem vencedor e só um jogador restante
        // (dword_56DEC4 == 1) → vence o local. A contagem de jogadores vivos
        // ainda não está portada.
    } else {
        win = 0;
    }
    if (win) {
        uint32_t winners = win;
        for (int p = 0; p < n; ++p)
            if (win & (1u << p)) winners |= g_playerData[p].allyMask;   // sub_490C80
        const bool mp = isMultiplayerType(mission);
        if (winners & (1u << local)) { endGame(mp ? 7 : 3, 2); soundVoice(0x87, -1, -1); }
        else { endGame(mp ? 8 : 4, 2); soundVoice(0x86, -1, -1); }
    }
    // 5) Derrota: FailureCondition habilitada com o bit do jogador local.
    for (int id : s_failure) {
        Trigger* t = trig(id);
        if (t && t->enabled && (t->playerMask & (1u << local))) endGame(4, 2);
    }
}

std::vector<std::string> missionObjectives() {
    std::vector<std::string> v;
    for (int id : s_victory) {
        Trigger* t = trig(id);
        const bool done = t && (t->playerMask & (1u << TethysGame::localPlayer()));   // sub_403450
        v.push_back(std::string(done ? "<C0>" : "<C2>") + (t && t->text ? t->text : ""));
    }
    return v;
}

} // namespace op2
