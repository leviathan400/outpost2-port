#include "tethys_game.h"
#include "colony.h"

namespace op2 {

uint32_t TethysGame::s_tick = 0;
int TethysGame::s_daylightPhase = 0;
int TethysGame::s_daylightPos = 0;
int TethysGame::s_daylightMoves = 0;
int TethysGame::s_daylightEverywhere = 0;
int      TethysGame::s_numPlayers = 0;
int      TethysGame::s_localPlayer = 0;
GameRNG  TethysGame::s_rng;
int      TethysGame::s_cheat[16] = {};
uint32_t TethysGame::s_startFlags = 0;

void TethysGame::reset(uint32_t seed, int numPlayers, int localPlayer) {
    s_tick = 0;
    s_daylightPhase = 0; s_daylightPos = 0;
    s_numPlayers = numPlayers;
    s_localPlayer = localPlayer;
    s_rng.setSeed(seed);
    for (int& c : s_cheat) c = 0;
}

void TethysGame::ForceMoraleGreat(int p)  { colonyForceMorale(p, 0); }
void TethysGame::ForceMoraleGood(int p)   { colonyForceMorale(p, 1); }
void TethysGame::ForceMoraleOK(int p)     { colonyForceMorale(p, 2); }
void TethysGame::ForceMoralePoor(int p)   { colonyForceMorale(p, 3); }
void TethysGame::ForceMoraleRotten(int p) { colonyForceMorale(p, 4); }
void TethysGame::FreeMoraleLevel(int p)   { colonyFreeMorale(p); }

} // namespace op2
