#include "player.h"

#include "colony.h"
#include "research.h"
#include "unit.h"

namespace op2 {

PlayerData g_playerData[kMaxPlayers];

// Inicializa o array global de wrappers com os índices 0..6 (como os _Player do
// original, cada um guardando seu índice).
_Player Player[kMaxPlayers] = {
    _Player(0), _Player(1), _Player(2), _Player(3),
    _Player(4), _Player(5), _Player(6),
};

void _Player::SetTechLevel(int v) {
    d().techLevel = v;
    researchSetTechLevel(m_index, v);
}
void _Player::MarkResearchComplete(int techID) { researchMarkComplete(m_index, techIndex(techID)); }
int _Player::HasTechnology(int techID) const { return researchHas(m_index, techIndex(techID)) ? 1 : 0; }

uint32_t playerSpaceWord(int player) {
    const PlayerData& d = g_playerData[player];
    return (static_cast<uint32_t>(d.rlvCount) & 0xFu) | ((static_cast<uint32_t>(d.solarSat) & 0xFu) << 4) |
           (d.spaceBits & ~0xFFu);
}
int playerSpaceCount(int player, int type) {                // sub_490800
    if (player < 0 || player >= kMaxPlayers) return 0;
    const uint32_t w = playerSpaceWord(player);
    if (type == 0x6A) return static_cast<int>(w & 0xF);
    if (type == 0x59) return static_cast<int>((w & 0xF0) >> 4);
    if (type == 0x58) return static_cast<int>((w & 0x1E00) >> 9);
    return 0;
}
int playerSpaceHas(int player, int type) {                  // sub_490780
    if (type == 0x6A || type == 0x59 || type == 0x58) return playerSpaceCount(player, type);
    if (player < 0 || player >= kMaxPlayers) return 0;
    return static_cast<int>((1u << (static_cast<uint8_t>(type - 0x4D) & 31)) & playerSpaceWord(player));
}

bool playerHasVehicle(int player, int type, int cargo) {    // ?hasVehicle
    for (int v : colonyVehicles(player)) {
        const UnitRecord& r = g_units[v];
        if (r.type == type && (cargo == -1 || static_cast<int16_t>(r.cargo) == cargo)) return true;
    }
    return false;                                            // (+ baias das garagens 0x29)
}

static bool playerHasBuilding(int player, int type) {
    for (int b : colonyStructures(player)) if (g_units[b].type == type) return true;
    return false;
}

int playerCanBuildBuilding(int p, signed char* c) {          // ?canBuildBuilding (+4)
    if (c[0] != -1) return c[0];
    if (!playerHasBuilding(p, 0x2F)) {                       // sem Structure Factory: kit dela numa ConVec
        c[0] = playerHasVehicle(p, 2, 0x2F) ? 1 : 0;
        return c[0];
    }
    if (playerHasVehicle(p, 2, -1)) { c[0] = 1; return 1; }
    int v;
    if (c[1] != -1) v = c[1];
    else if (playerHasBuilding(p, 0x30) || playerHasVehicle(p, 2, 0x30)) { c[1] = 1; v = 1; }
    else { c[1] = 0; v = 0; }
    c[0] = v ? 1 : 0;
    return c[0];
}

int playerCanBuildVehicle(int p, signed char* c, int arg) {   // ?canBuildVehicle (+5)
    if (c[1] != -1) return c[1];
    if (playerHasBuilding(p, 0x30) || playerHasVehicle(p, 2, 0x30)) { c[1] = 1; return 1; }
    if (!arg) { c[1] = 0; return 0; }
    c[1] = static_cast<signed char>(playerCanBuildBuilding(p, c));
    return c[1];
}

// Bloco repetido de canAccumulateOre: fábrica de veículos (ou kit) senão canBuildBuilding.
static int vehicleFactoryOrBuild(int p, signed char* c) {
    if (c[1] != -1) return c[1];
    if (playerHasBuilding(p, 0x30) || playerHasVehicle(p, 2, 0x30)) { c[1] = 1; return 1; }
    c[1] = static_cast<signed char>(playerCanBuildBuilding(p, c));
    return c[1];
}

int playerCanAccumulateOre(int p, signed char* c) {          // ?canAccumulateOre (+7)
    if (c[3] != -1) return c[3];
    const PlayerData& d = g_playerData[p];
    if (!d.canAccumulateOre && !playerHasVehicle(p, 9, -1) && !vehicleFactoryOrBuild(p, c)) { c[3] = 0; return 0; }
    if (!playerHasVehicle(p, 1, -1) && !vehicleFactoryOrBuild(p, c)) { c[3] = 0; return 0; }
    // +0x138 = total de smelters (estatística ST_SMELT_GRP).
    if (!g_colony[p].stat(ST_SMELT_GRP) && !playerCanBuildBuilding(p, c)) { c[3] = 0; return 0; }
    c[3] = 1;
    return 1;
}

int playerCanDoResearch(int p, signed char* c, int techID) { // ?canDoResearch (+9..+11)
    const Tech* t = techAt(techIndex(techID));
    const int lab = t ? t->lab : 0;
    int type = 0, slot = 0;                                  // lab fora de 1..3: cache [0], tipo 0
    if (lab == 1) { type = 0x21; slot = 5; }
    else if (lab == 2) { type = 0x31; slot = 6; }
    else if (lab == 3) { type = 0x32; slot = 7; }
    if (c[slot] != -1) return c[slot];
    if (playerHasBuilding(p, type) || playerHasVehicle(p, 2, type)) { c[slot] = 1; return 1; }
    c[slot] = static_cast<signed char>(playerCanBuildBuilding(p, c));
    return c[slot];
}

void resetPlayers(int numPlayers) {
    (void)numPlayers;
    for (int i = 0; i < kMaxPlayers; ++i) {
        g_playerData[i] = PlayerData{};   // volta aos defaults
        // sub_490190: [+4] = (1 << jogador) | 0x40 — aliado de si mesmo e do
        // jogador 6 (gaia, dono dos projéteis/efeitos).
        g_playerData[i].allyMask = (1u << i) | 0x40u;
    }
}

} // namespace op2
