#include "start_info.h"

#include "colony.h"
#include "missile.h"
#include "player.h"
#include "research.h"
#include "tethys_game.h"
#include "unit.h"
#include "../mission/mission.h"

namespace op2 {

namespace {

// [jogador+0x08] do port (rlvCount / solarSat / spaceBits) ↔ palavra única.
void setSpaceWord(PlayerData& d, uint32_t w) {
    d.rlvCount = static_cast<int>(w & 0xF);
    d.solarSat = static_cast<int>((w >> 4) & 0xF);
    d.spaceBits = w & ~0xFFu;
}

} // namespace

void startInfoApplyCampaign(const GameStartInfo& gsi) {      // sub_4898F0
    const int p = TethysGame::localPlayer();
    PlayerData& d = g_playerData[p];
    d.foodStored = gsi.food;                                 // +0x10
    d.ore = gsi.ore;                                         // +0x20
    d.rareOre = gsi.rareOre;                                 // +0x24
    d.workers = gsi.workers;                                 // +0x94
    d.scientists = gsi.scientists;                           // +0x98
    d.kids = gsi.kids;                                       // +0x9C
    setSpaceWord(d, gsi.spaceWord);                          // +0x08
    // sub_490800(0x6A): um RLV em órbita por RLV contado, chegando de 100 em 100.
    const int rlv = playerSpaceCount(p, 0x6A);
    for (int k = 1; k <= rlv; ++k) missileCampaignRLV(p, 100 * k);
    colonyPlayerBlockLoad(p, gsi.playerBlock);               // +0x34..+0x7B
    researchLoadResults(gsi.techCount, gsi.techIdx, gsi.techMask);   // sub_473770
}

void startInfoCollectResults(GameStartInfo& gsi, const AIModDesc* desc, int endCode) {   // sub_489A30
    gsi.endCode = endCode;                                   // +0xBA
    if (!desc) gsi.missionNumber = -1;
    else if (!gsi.campaign()) gsi.missionNumber = desc->missionType;
    gsi.techLevel = 0;                                       // +0x1DB
    gsi.endTick = TethysGame::tick();                        // +0x18B
    const int p = TethysGame::localPlayer();
    gsi.localPlayer = p;                                     // +0x18A
    // loc_489CF3: os sete dwords de cada jogador, com o ultimo trocado pela
    // soma dos minerios. Isto vem ANTES do desvio da campanha, como no
    // original -- o laco e a ultima coisa que sub_489A30 faz.
    for (int i = 0; i < 6; ++i) {
        const PlayerData& q = g_playerData[i];
        gsi.stats[i][0] = q.structuresBuilt;
        gsi.stats[i][1] = q.unitsBuilt;
        gsi.stats[i][2] = q.structuresLost;
        gsi.stats[i][3] = q.unitsLost;
        gsi.stats[i][4] = q.enemyStructuresDestroyed;
        gsi.stats[i][5] = q.enemyUnitsDestroyed;
        gsi.stats[i][6] = q.ore + q.rareOre;
    }
    if (gsi.campaign() && endCode != 3) return;
    PlayerData& d = g_playerData[p];
    gsi.food = d.foodStored;
    gsi.ore = d.ore;
    gsi.rareOre = d.rareOre;
    gsi.workers = d.workers;
    gsi.scientists = d.scientists;
    gsi.kids = d.kids;
    if (p >= 0 && p < 6)
        gsi.playerWord[p] = (gsi.playerWord[p] & ~0x18u) | ((static_cast<uint32_t>(d.difficulty) << 3) & 0x18u);
    // Spaceports com RLV na plataforma: o RLV volta, e a carga dele conta como
    // lançada (EDWARD e satélites somam; módulos 0x5A..0x63 viram bits).
    for (int b : colonyStructures(p)) {
        const UnitRecord& s = g_units[b];
        if (s.type != 0x37 || s.padCargo != 0x6A) continue;
        uint32_t w = playerSpaceWord(p);
        int n = playerSpaceCount(p, 0x6A) + 1;
        if (n > 15) n = 15;
        w = (w & ~0xFu) | static_cast<uint32_t>(n);
        const int pay = s.padPayload;
        if (pay == 0x58) {
            int e = playerSpaceCount(p, 0x58) + 1;
            if (e > 15) e = 15;
            w = (w & 0xFFFFE1FFu) | (static_cast<uint32_t>(e) << 9);
        } else if (pay == 0x59) {
            int e = playerSpaceCount(p, 0x59) + 1;
            if (e > 15) e = 15;
            w = (w & 0xFFFFFF0Fu) | (static_cast<uint32_t>(e) << 4);
        } else if (pay >= 0x5A && pay <= 0x63) {
            w |= 1u << (pay - 0x4D);
        }
        setSpaceWord(d, w);
    }
    gsi.spaceWord = playerSpaceWord(p);                      // +0x186
    colonyPlayerBlockSave(p, gsi.playerBlock);               // +0x18F
    researchSaveResults(gsi.techCount, gsi.techIdx, gsi.techMask);   // sub_473640
}

} // namespace op2
