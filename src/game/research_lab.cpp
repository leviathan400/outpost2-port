#include "research_lab.h"
#include "../audio/sound.h"

#include <cstdio>

#include "building_sheet.h"
#include "colony.h"
#include "messages.h"
#include "player.h"
#include "research.h"
#include "scenario.h"
#include "tethys_game.h"
#include "unit.h"

namespace op2 {

// byte_4E0CCD: eficiência por (máximo do projeto + cientistas*15), bytes com
// sinal lidos do OUTPOST2.EXE (0x4E0CCD .. +0x12C).
static const signed char kEff[300] = {
    0,0,0,37,115,32,37,115,32,35,32,37,105,45,37,
    48,50,105,0,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,9,9,9,8,8,8,8,8,8,8,8,
    8,8,8,8,11,11,10,9,9,9,9,9,9,9,9,
    9,9,9,9,13,12,11,10,10,10,10,10,10,10,10,
    10,9,9,9,13,13,12,11,11,11,11,10,10,10,10,
    10,10,10,9,13,13,13,12,12,11,11,11,11,11,11,
    10,10,10,10,13,13,13,13,12,12,12,11,11,11,11,
    11,10,10,10,13,13,13,13,13,12,12,12,11,11,11,
    11,11,10,10,13,13,13,13,13,13,12,12,12,12,11,
    11,11,11,10,13,13,13,13,13,13,13,12,12,12,12,
    11,11,11,11,13,13,13,13,13,13,13,13,12,12,12,
    12,11,11,11,13,13,13,13,13,13,13,13,13,12,12,
    12,12,11,11,13,13,13,13,13,13,13,13,13,13,12,
    12,12,12,11,13,13,13,13,13,13,13,13,13,13,13,
    12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,
    13,12,12,12,13,13,13,13,13,13,13,13,13,13,13,
    13,13,12,12,13,13,13,13,13,13,13,13,13,13,13,
    13,13,13,12,13,13,13,13,13,13,13,13,13,13,13,
    13,13,13,13,0,0,109,111,73,110,118,97,108,105,100,
};

static bool valid(int id) { return id > 0 && id < static_cast<int>(g_units.size()) && g_units[id].id != -1; }
static int ownerOf(const UnitRecord& r) { return r.owner & 0x0F; }

int labResearchKind(int labType) {
    return labType == 0x21 ? 1 : labType == 0x31 ? 2 : labType == 0x32 ? 3 : 0;
}

std::vector<int> labTopics(int labId) {                 // sub_4731F0
    std::vector<int> out;
    if (!valid(labId)) return out;
    const int kind = labResearchKind(g_units[labId].type);
    const int p = TethysGame::localPlayer();
    for (int i = 1; i < techCount(); ++i) {
        if (!researchAvailable(p, i)) continue;          // sub_473300
        if (kind && techAt(i)->lab != kind) continue;
        out.push_back(i);
    }
    return out;
}

bool processStartResearch(int owner, int labId, int topic, int scientists) {   // case 22
    if (!valid(labId)) return false;
    UnitRecord& r = g_units[labId];
    if (ownerOf(r) != (owner & 0x0F)) return false;
    // (Humano: anel anti-duplicata [jogador+0xBD8] — rede, não portado.)
    if (r.action != 0 && r.action != 0x16) return false;
    if (researchHas(ownerOf(r), topic)) return false;    // sub_472C50
    if (!(r.flags & UNIT_DEAD) && structureOperational(r)) {
        if (r.action != 0x16) { r.action = 0x16; r.field20 = 1; r.field3C = 0; }
        r.labTopic = topic;                              // [+0x60]
        r.extraScientists = static_cast<int8_t>(scientists);   // [+0x64]
    }
    colonyMarkDirty(ownerOf(r));                         // dword_56E37C = 1
    return true;
}

void sendStartResearch(int labId, int topic, int scientists) {   // sub_457610
    const int p = TethysGame::localPlayer();
    if (!researchAvailable(p, topic)) return;            // sub_473300
    processStartResearch(p, labId, topic, scientists);
}

void labResearchStart(int labId) {                      // sub_43ACA0 case 22 ([+0x20])
    UnitRecord& r = g_units[labId];
    r.field20 = 0;
    if (r.field22 == 0x0C) return;                      // já pesquisando
    r.field22 = 0x0C;
    const Tech* t = techAt(r.labTopic);
    // Custo da facção do prédio ([+0x44] & 0x1000 → Eden +0x10, senão Plymouth +0x0C).
    const bool eden = ownerOf(r) < kMaxPlayers && g_playerData[ownerOf(r)].isEden;
    const int cost = t ? (eden ? t->edenCost : t->plymouthCost) : 0;
    if (TethysGame::cheatFlag(5)) { r.field3C = 1; r.cargo = 1; return; }   // dword_56DE6C
    r.field3C = cost << 8;
    r.cargo = 0xC0;                                      // [+0x24] word
}

void labResearchProgress(int labId) {                   // sub_43DE50 case 12
    UnitRecord& r = g_units[labId];
    if (!structureOperational(r)) return;
    if (--r.cargo != 0) return;
    r.cargo = 0xC0;
    const Tech* t = techAt(r.labTopic);
    const int sci = r.extraScientists;
    const int maxSci = t ? t->maxScientists : 0;
    const int idx = maxSci + sci * 15;
    int pts = (idx >= 0 && idx < 300) ? kEff[idx] : 0;
    const int bonus = moraleRow(g_playerData[ownerOf(r)].morale).rschBonus;   // dword_564658
    pts = pts * (bonus + 0x100) * sci;
    if (multiplayerMission()) pts *= 3;
    r.field3C -= pts;
    if (r.field3C > 0) return;
    r.field23 = r.field22;
    r.field22 = 0;
}

void labResearchFinish(int labId) {                     // jpt_43B312 case 22
    UnitRecord& r = g_units[labId];
    const int p = ownerOf(r);
    if (p == TethysGame::localPlayer()) {
        const Tech* t = techAt(r.labTopic);
        char msg[100];
        std::snprintf(msg, sizeof(msg), "%s research completed", t ? t->name.c_str() : "");
        postMessage(r.pixelX + 16, r.pixelY + 16, msg, r.type == 0x21 ? 0x69 : 0x5F);
    }
    researchMarkComplete(p, r.labTopic);                 // sub_473830
    moraleNewTech(p);                                    // sub_44F840 (NEW_TECH_BORN)
    r.extraScientists = 0;                               // [+0x64]
    colonyMarkDirty(p);
    r.labTopic = 0;                                      // [+0x60]
    r.action = 0;
}

bool processTrainPacket(int owner, int univId, int students) {   // case 23
    if (!valid(univId)) return false;
    UnitRecord& r = g_units[univId];
    if (ownerOf(r) != (owner & 0x0F)) return false;
    if (r.action != 0) return false;
    if (!(r.flags & UNIT_DEAD) && structureOperational(r)) {
        r.action = 0x17; r.field20 = 1; r.field3C = 0;
        r.extraWorkers = static_cast<int8_t>(students);  // [+0x60] (byte com sinal)
        const StructSheetDesc* d = structSheet(r.type);
        int t = d ? d->player[(r.owner >> 4) & 0x0F].storageCapacity : 0;   // bloco+0x30
        if (multiplayerMission()) t = (t - (t >> 31)) >> 1;
        r.trainTime = t;                                 // [+0x64]
    }
    colonyMarkDirty(ownerOf(r));
    return true;
}

void sendTrain(int univId, int students) {
    processTrainPacket(TethysGame::localPlayer(), univId, students);
}

void universityTrainStart(int univId) {                 // sub_43ACA0 case 23 ([+0x20])
    UnitRecord& r = g_units[univId];
    r.field20 = 0;
    if (r.field22 == 7) return;
    r.field22 = 7;
    r.field3C = TethysGame::cheatFlag(5) ? 1 : r.trainTime;   // dword_56DE6C
}

void universityTrainFinish(int univId) {                // jpt_43B312 case 23
    UnitRecord& r = g_units[univId];
    const int p = ownerOf(r);
    if (p == TethysGame::localPlayer()) {                   // 0x3A + sub_439180 (beep)
        soundPlay(0x3A);
        postMessage(r.pixelX + 16, r.pixelY + 16, "Scientist training completed", 0);
    }
    const int n = r.extraWorkers;
    g_playerData[p].workers -= n;                        // dword_56E370
    g_playerData[p].scientists += n;                     // dword_56E374
    moralePhdTrained(p, n);                              // sub_44F330
    r.extraWorkers = 0;
    colonyMarkDirty(p);
    r.action = 0;
}

void structureStopTick(int id) {                        // sub_43ACA0 case 5 (prédio)
    UnitRecord& r = g_units[id];
    r.action = 0;
    r.field22 = 0;
    if (labResearchKind(r.type)) {                       // sub_432060
        r.extraScientists = 0;
        r.labTopic = 0;
        colonyMarkDirty(ownerOf(r));
    } else if (r.type == 0x26) {                         // sub_49D610 (University)
        r.extraWorkers = 0;
        colonyMarkDirty(ownerOf(r));
    }
    r.field20 = 0;
}

} // namespace op2
