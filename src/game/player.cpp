#include "player.h"

#include "research.h"

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
