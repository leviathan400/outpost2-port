#include "pinwheel.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "ai_group.h"
#include "colony.h"
#include "game_map.h"
#include "player.h"
#include "scenario.h"
#include "tethys_game.h"
#include "save_stream.h"
#include "trigger.h"
#include "unit.h"

namespace op2 {

namespace {

struct MrRec { int type = 0, weapon = 0, cum = 0, minCount = 0; };   // 16 bytes (sub_45D380)
struct Comp {                                                        // +0x64 / +0x74 / +0x84
    int lo = 0, hi = 0;                                              // [+0] / [+4]
    std::vector<MrRec> recs;                                         // [+8] n, ponteiro em +0x28/+0x2C/+0x30
    int sum = 0;                                                     // [+0xC]
};
struct Point {                                                       // 0x3C bytes (+0x24)
    int grp[3] = {0xFF, 0xFF, 0xFF};                                 // +0x00: ataque, guarda, sapador
    int rect1[4] = {}, rect2[4] = {};                                // +0x0C / +0x1C (px internos)
    int link[2] = {};                                                // +0x2C / +0x30 (ligação != −1)
    int weight = 0;                                                  // +0x34
    int time = -1;                                                   // +0x38 (horário da geração)
};

struct Pinwheel : Stub {
    std::vector<Point> pts;                                          // +0x24 (n = +0x34)
    int maxWeight = 0, maxLink = 0;                                  // +0x38 / +0x3C
    int player = 0;                                                  // +0x40
    int contactDelay = 0;                                            // +0x44
    int state = 0;                                                   // +0x48
    int next = 0;                                                    // +0x4C
    int stealth = 0;                                                 // +0x50 (onda sem luzes)
    int waveMax = 0, waveMin = 0;                                    // +0x54 / +0x58
    int noMax = 0, noMin = 0;                                        // +0x5C / +0x60
    Comp comp[3];                                                    // +0x64 / +0x74 / +0x84
    int fraction = 0;                                                // +0x94
    int strength = 0;                                                // +0x98
    int saveKind() const override { return 4; }
    void saveIo(SaveStream& s) override {
        s.ioVec(pts);
        s.io(maxWeight); s.io(maxLink); s.io(player); s.io(contactDelay); s.io(state); s.io(next);
        s.io(stealth); s.io(waveMax); s.io(waveMin); s.io(noMax); s.io(noMin);
        for (Comp& c : comp) { s.io(c.lo); s.io(c.hi); s.ioVec(c.recs); s.io(c.sum); }
        s.io(fraction); s.io(strength);
    }
};

std::vector<int> s_strategies;          // dword_4E4194 (cabeça = mais nova)

Pinwheel* pw(int stub) { return dynamic_cast<Pinwheel*>(stubObject(stub)); }
int tick() { return TethysGame::tick(); }
int rnd(int n) { return TethysGame::rng().getRand(n); }             // sub_46EF50

int newFightGroup(int player) { return aiCreateGroup(GK_FIGHT, player); }   // sub_47B460(off_4E0A58)
void destroyGroup(int stub) {                                        // vt[7] (delete)
    aiGroupDestroyed(stub);
    stubDestroy(stub);
}

// SetMode + vt[9] + [+0x344] = rect (+ empilha como guardado) + SetLights.
void orderAll(Pinwheel& w, int mode, bool second, bool guard, int lights) {
    for (Point& p : w.pts)
        for (int g = 0; g < 3; ++g) {
            aiFightSetMode(p.grp[g], mode);
            aiFightSetRectPx(p.grp[g], second ? p.rect2 : p.rect1, guard);
            aiSetLights(p.grp[g], lights);
        }
}

// sub_45E390: junta guarda e sapador no grupo de ataque de cada ponto; os que
// sobraram ficam nesse grupo (apagado quando esvaziar) e o ponto ganha outro.
void merge(Pinwheel& w) {
    for (Point& p : w.pts) {
        aiTakeAllUnits(p.grp[0], p.grp[1]);
        aiTakeAllUnits(p.grp[0], p.grp[2]);
        if (aiTotalUnitCount(p.grp[0])) {
            aiSetDeleteWhenEmpty(p.grp[0], 1);
            p.grp[0] = newFightGroup(w.player);
        }
    }
}

// sub_45D420: gera a composição no retângulo e põe no grupo. Total entre
// lo*força e hi*força (16.16, arredondado; máx. 32; 1 se algum der 0): primeiro
// os mínimos de cada registro, depois sorteio pelo peso acumulado.
void spawnComp(Pinwheel& w, const Comp& c, int group, const int rc[4]) {
    const int pxW = g_gameMap.internalWidth() * 32, pxMask = pxW - 1;
    int dx = rc[2] - rc[0];
    const int half = pxMask / 2;
    if (-half >= dx) dx += pxW;
    else if (half < dx) dx = -((-dx) & pxMask);
    const int dy = rc[3] - rc[1];
    auto fix16 = [](int a, int s) {                                  // (a*s + 0x7FFF) >> 16, com sinal
        const int v = a * s;
        return (v + 0x7FFF + ((v >> 31) & 0xFFFF)) >> 16;
    };
    const int hi = fix16(c.hi, w.strength), lo = fix16(c.lo, w.strength);
    int total = 1;
    if (lo && hi) total = std::min(lo + rnd(hi - lo + 1), 0x20);
    auto spawn = [&](const MrRec& r) {
        const int px = (rc[0] + rnd(dx)) & pxMask;
        const int py = rc[1] + rnd(dy);
        int tx = ((px >> 5) - g_gameMap.internalXOffset()) & (g_gameMap.width() - 1), ty = py >> 5;
        findVehicleSpot(r.type, tx, ty);                             // sub_4366D0
        Unit u;                                                      // sub_4466B0
        if (createUnit(u, static_cast<map_id>(r.type), LOCATION{tx, ty}, w.player, static_cast<map_id>(r.weapon)))
            aiTakeUnit(group, u.id());                               // vt[10]
    };
    for (const MrRec& r : c.recs)
        for (int k = 0; k < r.minCount; ++k) { spawn(r); --total; }
    while (total > 0) {
        const int v = rnd(c.sum);
        size_t i = 0;
        while (i + 1 < c.recs.size() && c.recs[i].cum <= v) ++i;
        spawn(c.recs[i]);
        --total;
    }
}

// sub_45E300: gera as três composições em rect1 do ponto e acende/apaga.
void spawnAt(Pinwheel& w, Point& p, int lights) {
    if (!w.comp[0].recs.empty()) spawnComp(w, w.comp[0], p.grp[0], p.rect1);
    if (!w.comp[1].recs.empty()) spawnComp(w, w.comp[1], p.grp[1], p.rect1);
    if (!w.comp[2].recs.empty()) spawnComp(w, w.comp[2], p.grp[2], p.rect1);
    for (int g = 0; g < 3; ++g) aiSetLights(p.grp[g], lights);
}

// sub_45D7C0: ataque e sapador em AttackEnemy (rect2); a guarda escolta o
// sapador (ou o ataque, se o sapador está vazio); o sapador mira um tipo de
// prédio que algum inimigo tenha (até 14 sorteios da tabela dword_4D53E8).
void attack(Pinwheel& w, Point& p) {
    static const int kSapperTargets[7] = {0x37, 0x17, 0x15, 0x2F, 0x2F, 0x30, 0x32};
    aiFightSetMode(p.grp[0], 5);
    aiFightSetRectPx(p.grp[0], p.rect2, false);
    aiSetLights(p.grp[0], 1);
    aiFightSetMode(p.grp[1], 2);
    aiFightSetRectPx(p.grp[1], p.rect2, false);
    aiFightSetFollowMode(p.grp[1], 1);                               // [+0x340]
    aiFightSetTargetGroup(p.grp[1], aiTotalUnitCount(p.grp[2]) ? p.grp[2] : p.grp[0]);   // [+0x354]
    aiSetLights(p.grp[1], 1);
    aiFightSetMode(p.grp[2], 5);
    aiFightSetRectPx(p.grp[2], p.rect2, false);
    aiSetLights(p.grp[2], 1);
    if (!aiTotalUnitCount(p.grp[2])) return;
    const int n = TethysGame::noPlayers();
    for (int tries = 0; tries < 0x0E; ++tries) {
        const int type = kSapperTargets[rnd(7)];
        for (int j = 0; j < n && j < kMaxPlayers; ++j) {
            if (g_playerData[j].allyMask & (1u << w.player)) continue;   // sub_490C50
            for (int id : colonyStructures(j))
                if (g_units[id].type == type) { aiFightSetAttackType(p.grp[2], type); return; }   // [+0x358]
        }
    }
}

// sub_45E420: prepara a onda.
void prepare(Pinwheel& w, int count) {
    merge(w);
    w.stealth = TethysGame::daylightEverywhere() ? 0 : (rnd(100) < 0x19 ? 1 : 0);
    for (Point& p : w.pts) {
        for (int g = 0; g < 3; ++g) {
            aiFightSetMode(p.grp[g], 0);                             // GuardRect em rect2
            aiFightSetRectPx(p.grp[g], p.rect2, true);
            aiSetLights(p.grp[g], w.stealth ? 0 : 1);
        }
        p.time = -1;
    }
    if (count == -1) count = w.noMin + rnd(w.noMax - w.noMin + 1);
    // Força alvo: fração × força do jogador 0 / (soma das composições / 2 × pontos).
    const int s = playerTotalStrength(0) * w.fraction;
    const int den = ((w.comp[0].lo + w.comp[0].hi + w.comp[1].lo + w.comp[1].hi + w.comp[2].lo + w.comp[2].hi) / 2) * count;
    w.strength = den ? s / den : 0x4000;      // (o original divide por zero se não houver composição/pontos)
    if (w.strength >= 0xA0000) w.strength = 0xA0000;
    if (w.strength <= 0x4000) w.strength = 0x4000;
    for (int k = 0; k < count; ++k) {
        // Sorteia um ponto ainda sem horário (o original repete até achar).
        if (std::none_of(w.pts.begin(), w.pts.end(), [](const Point& p) { return p.time == -1; })) break;
        int i;
        do i = rnd(static_cast<int>(w.pts.size())); while (w.pts[i].time != -1);
        Point& p = w.pts[i];
        p.time = w.maxWeight - p.weight;
        if (w.stealth) p.time += p.time / 2;
        p.time += tick();
    }
    w.next = tick();
    w.state = 1;
}

// sub_45E6A0 (vt[9]).
void think(Pinwheel& w) {
    const int n = static_cast<int>(w.pts.size());
    if (w.next > tick()) {                                           // loc_45EDD5
        if (w.state <= 1) return;
        for (const Point& p : w.pts)
            for (int g = 0; g < 3; ++g) if (aiTotalUnitCount(p.grp[g])) return;
        if (w.state != 4) orderAll(w, 6, false, false, 1);
        w.state = 0;
        w.next = rnd(w.waveMax - w.waveMin + 1) + w.maxWeight / 2 + w.waveMin + tick();
        return;
    }
    const int r = rnd(100);
    switch (w.state) {
        case 0:
            prepare(w, -1);
            return;
        case 1: {
            int done = 0;
            for (Point& p : w.pts) {
                if (p.time == -1) ++done;
                else if (p.time <= tick()) {
                    p.time = -1;
                    spawnAt(w, p, w.stealth ? 0 : 1);
                    ++done;
                }
                if (done == n) {
                    w.state = 2;
                    int t = rnd(w.maxWeight / 4) + w.maxWeight * 7 / 8;
                    if (w.stealth) t += t / 2;
                    w.next += t;
                }
            }
            return;
        }
        case 2:
            if (r < 0x0A) {                                          // recua
                orderAll(w, 6, false, false, 1);
                w.state = 4;
                w.next += w.maxWeight / 2;
            } else if (r < 0x11) {
                w.next += w.maxWeight / 2;
            } else if (r < 0x1E && n > 1) {                          // gira as tropas entre os pontos
                const int d = static_cast<int>(TethysGame::rng().next() & 1);   // sub_46EEF0
                int temp[3] = {0xFF, 0xFF, 0xFF};
                if (w.pts[0].link[d])
                    for (int g = 0; g < 3; ++g) {
                        temp[g] = newFightGroup(w.player);
                        aiTakeAllUnits(temp[g], w.pts[0].grp[g]);
                    }
                int prev = 0;
                for (int k = 1; k < n; ++k) {
                    const int j = ((d ? prev - 1 : prev + 1) + n) % n;
                    if (w.pts[j].link[d])
                        for (int g = 0; g < 3; ++g) aiTakeAllUnits(w.pts[prev].grp[g], w.pts[j].grp[g]);
                    prev = j;
                }
                if (w.pts[0].link[d]) {
                    const int last = d ? 1 : n - 1;
                    for (int g = 0; g < 3; ++g) {
                        aiTakeAllUnits(w.pts[last].grp[g], temp[g]);
                        destroyGroup(temp[g]);
                    }
                }
                w.next += w.maxLink;
            } else {                                                 // ataca
                for (Point& p : w.pts) attack(w, p);
                w.next += w.contactDelay;
                w.state = 3;
            }
            return;
        case 3:
            if (r < 5) {
                orderAll(w, 6, false, false, 1);
                w.state = 4;
                w.next += w.maxWeight / 2 + w.contactDelay / 2;
            } else if (r < 0x14) {
                orderAll(w, 0, true, true, 1);
                w.state = 2;
                w.next += w.contactDelay / 2;
            } else if (r < 0x3C) {
                merge(w);
                orderAll(w, 6, false, false, 1);
                w.state = 0;
                w.next += rnd(w.waveMax - w.waveMin + 1) + w.maxWeight + w.waveMin;
            } else {
                w.next += w.contactDelay;
            }
            return;
        case 4:
            if (r < 0x0A) {
                orderAll(w, 0, true, true, 1);
                w.state = 2;
            }
            if (r < 0x0F) {
                for (Point& p : w.pts) attack(w, p);
                w.state = 3;
                w.next += w.contactDelay + w.maxWeight / 2;
            } else {
                w.state = 0;
                w.next += rnd(w.waveMax - w.waveMin + 1) + w.maxWeight + w.waveMin;
            }
            return;
        default:
            return;
    }
}

} // namespace

int pinwheelCreate(int player) {
    auto* w = new Pinwheel();
    w->player = player;                                              // [+0x40]
    const int id = stubAllocate(w);
    if (id != 0xFF) s_strategies.insert(s_strategies.begin(), id);   // sub_47B4F0
    return id;
}

void pinwheelSetPoints(int stub, const int* defs) {                 // sub_45DFA0
    Pinwheel* w = pw(stub);
    if (!w || !defs) return;
    if (w->state) {
        if (w->state != 3 && w->state != 4)
            for (Point& p : w->pts) attack(*w, p);
        merge(*w);
    }
    for (Point& p : w->pts)
        for (int g = 0; g < 3; ++g) destroyGroup(p.grp[g]);          // vt[7]
    w->pts.clear();
    w->maxWeight = 0; w->maxLink = 0; w->state = 0;
    w->next = 0x7FFFFFFF;
    for (const int* d = defs; d[8] >= 0; d += 11) {
        Point p;
        for (int k = 0; k < 4; ++k) { p.rect1[k] = d[k] << 5; p.rect2[k] = d[4 + k] << 5; }
        p.weight = d[8];
        p.link[0] = d[9] != -1;
        p.link[1] = d[10] != -1;
        w->maxWeight = std::max(w->maxWeight, p.weight);
        w->maxLink = std::max(std::max(w->maxLink, d[9]), d[10]);
        for (int g = 0; g < 3; ++g) {
            p.grp[g] = newFightGroup(w->player);
            aiFightSetMode(p.grp[g], 6);
            aiFightSetRectPx(p.grp[g], p.rect1, false);
            aiSetLights(p.grp[g], 1);
        }
        w->pts.push_back(p);
    }
}

void pinwheelSetContactDelay(int stub, int d) { if (Pinwheel* w = pw(stub)) w->contactDelay = d; }
void pinwheelSetAttackFraction(int stub, int f) { if (Pinwheel* w = pw(stub)) w->fraction = f; }
void pinwheelSetNoRange(int stub, int lo, int hi) {
    if (Pinwheel* w = pw(stub)) { w->noMin = lo; w->noMax = hi; }
}
void pinwheelSetWavePeriod(int stub, int lo, int hi) {              // sub_45E220
    Pinwheel* w = pw(stub);
    if (!w) return;
    w->waveMin = lo; w->waveMax = hi;
    if (w->next == 0x7FFFFFFF) w->next = rnd(hi - lo + 1) + lo + tick();
}
void pinwheelSetComp(int stub, int comp, int lo, int hi, const int* recs) {   // sub_45D380
    Pinwheel* w = pw(stub);
    if (!w || comp < 0 || comp > 2) return;
    Comp& c = w->comp[comp];
    c.lo = lo; c.hi = hi;
    c.recs.clear();
    c.sum = 0;
    if (!recs) return;
    for (const int* r = recs; r[0] != 0; r += 4) {
        c.sum += r[2];
        MrRec m;
        m.type = r[0]; m.weapon = r[1]; m.cum = c.sum; m.minCount = r[3];
        c.recs.push_back(m);
    }
}

void strategiesTick() {                                             // sub_403110: dword_4E4194 → vt[9]
    for (int id : std::vector<int>(s_strategies))
        if (Pinwheel* w = pw(id)) think(*w);
}

void strategiesReset() { s_strategies.clear(); }
Stub* pinwheelNewForLoad() { return new Pinwheel(); }
void strategiesSerializeLists(SaveStream& s) { s.ioVec(s_strategies); }

void pinwheelDump() {
    for (int id : s_strategies) {
        const Pinwheel* w = pw(id);
        if (!w) continue;
        std::printf("[Pinwheel] stub %d jogador %d estado %d proximo %d (agora %d) furtiva %d forca 0x%X pontos %d\n",
                    id, w->player, w->state, w->next, tick(), w->stealth, w->strength, static_cast<int>(w->pts.size()));
        for (const Point& p : w->pts)
            std::printf("[Pinwheel]   ponto peso %d hora %d membros %d/%d/%d\n", p.weight, p.time,
                        aiTotalUnitCount(p.grp[0]), aiTotalUnitCount(p.grp[1]), aiTotalUnitCount(p.grp[2]));
    }
}

} // namespace op2
