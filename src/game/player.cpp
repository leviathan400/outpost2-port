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
