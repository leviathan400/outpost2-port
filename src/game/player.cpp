#include "player.h"

namespace op2 {

PlayerData g_playerData[kMaxPlayers];

// Inicializa o array global de wrappers com os índices 0..6 (como os _Player do
// original, cada um guardando seu índice).
_Player Player[kMaxPlayers] = {
    _Player(0), _Player(1), _Player(2), _Player(3),
    _Player(4), _Player(5), _Player(6),
};

void resetPlayers(int numPlayers) {
    (void)numPlayers;
    for (int i = 0; i < kMaxPlayers; ++i)
        g_playerData[i] = PlayerData{};   // volta aos defaults
}

} // namespace op2
