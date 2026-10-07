#include "tethys_game.h"

namespace op2 {

uint32_t TethysGame::s_tick = 0;
int TethysGame::s_daylightPhase = 0;
int TethysGame::s_daylightPos = 0;
int TethysGame::s_daylightMoves = 0;
int TethysGame::s_daylightEverywhere = 0;
int      TethysGame::s_numPlayers = 0;
int      TethysGame::s_localPlayer = 0;
GameRNG  TethysGame::s_rng;

void TethysGame::reset(uint32_t seed, int numPlayers, int localPlayer) {
    s_tick = 0;
    s_daylightPhase = 0; s_daylightPos = 0;
    s_numPlayers = numPlayers;
    s_localPlayer = localPlayer;
    s_rng.setSeed(seed);
}

} // namespace op2
