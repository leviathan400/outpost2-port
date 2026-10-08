#include "trigger.h"

#include <cstdio>
#include <iostream>
#include <memory>

#include "player.h"
#include "tethys_game.h"
#include "../mission/mission.h"

namespace op2 {

// ---- Tabela de stubs (dword_56B71C[255], último usado dword_56B718) -----------
static const int kMaxStubs = 0xFF;

struct Stub {
    virtual ~Stub() = default;
    int id = 0xFF;          // +0x04
    int enabled = 0;        // +0x08
};

// FuncReference (off_4E3C58; sub_475710): nome copiado (+0x20), origem (+0x1C:
// 1 = script da missão) e endereço resolvido (+0x24).
struct FuncReference : Stub {
    std::string name;
    int fromScript = 0;
    void (*fn)() = nullptr;
};

enum TriggerKind { TK_TIME, TK_RESEARCH, TK_VICTORY, TK_FAILURE };

// _Trigger (off_4E9E50) e derivadas. Campos +0x30/+0x34 conforme a classe.
struct Trigger : Stub {
    TriggerKind kind = TK_TIME;
    int oneShot = 0;           // +0x1C
    uint32_t playerMask = 0;   // +0x20
    int funcRef = 0;           // +0x24 (stubIndex da FuncReference; 0 = nenhuma)
    int f30 = 0, f34 = 0;      // +0x30 / +0x34
    const char* text = nullptr;  // Victory/FailureCondition: +0x30 (texto da DLL)
};

static Stub* s_stubs[kMaxStubs] = {};
static int s_lastStub = 0;
// Listas de instâncias (inserção na cabeça): _Trigger (dword_4E9E64),
// VictoryCondition (dword_4EA0C4), FailureCondition (dword_4EA0E4).
static std::vector<int> s_triggers, s_victory, s_failure;

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

void stubEnable(int stub, bool on) {                       // vt[3]/vt[4]
    if (stub >= 0 && stub < kMaxStubs && s_stubs[stub]) s_stubs[stub]->enabled = on ? 1 : 0;
}
bool stubEnabled(int stub) {
    return stub >= 0 && stub < kMaxStubs && s_stubs[stub] && s_stubs[stub]->enabled;
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
    // 2) Grupos (dword_4E4194) e IA (dword_4E0B64) — não portados.
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
        if (winners & (1u << local)) endGame(mp ? 7 : 3, 2);   // som 0x87
        else endGame(mp ? 8 : 4, 2);                          // som 0x86
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
