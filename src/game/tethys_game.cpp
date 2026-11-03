#include "tethys_game.h"

#include <SDL.h>
#include "colony.h"
#include "save_game.h"
#include "save_stream.h"

namespace op2 {

// --- as taxas de sub_467760 -------------------------------------------------
// Dois contadores e uma janela de um segundo; `s_speed`/`s_update` guardam a
// ultima janela fechada, para o numero nao tremer na tela.
namespace {
int   s_frameCount = 0, s_tickCount = 0;
float s_window = 0.0f, s_speed = 0.0f, s_update = 0.0f;
unsigned s_lastMs = 0;
}  // namespace

void TethysGame::noteFrame() {
    ++s_frameCount;
    const unsigned agora = SDL_GetTicks();
    if (s_lastMs == 0) { s_lastMs = agora; return; }
    s_window += (agora - s_lastMs) / 1000.0f;
    s_lastMs = agora;
    if (s_window < 1.0f) return;
    s_update = s_frameCount / s_window;
    s_speed = s_tickCount / s_window;
    s_frameCount = s_tickCount = 0;
    s_window = 0.0f;
}

void TethysGame::noteTick() { ++s_tickCount; }

void TethysGame::rates(float& gameSpeed, float& updateRate) {
    gameSpeed = s_speed;
    updateRate = s_update;
}


uint32_t TethysGame::s_tick = 0;
int TethysGame::s_daylightPhase = 0;
int TethysGame::s_daylightPos = 0;
int TethysGame::s_daylightMoves = 0;
int TethysGame::s_daylightEverywhere = 0;
int      TethysGame::s_numPlayers = 0;
int      TethysGame::s_localPlayer = 0;
GameRNG  TethysGame::s_rng;
int      TethysGame::s_cheat[16] = {};

void TethysGame::serialize(SaveStream& s) {
    s.io(s_tick);
    // A FASE e a coluna do dia nao moram aqui: sao os campos [+0x48] e [+0x44]
    // do objeto do globo, e vao no bloco de sub_420580 (minimapaStateSave).
    // Aqui ficam so as duas opcoes, que sao do objeto do jogo (dword_56DEA0 /
    // dword_56DEA4).
    s.io(s_daylightMoves); s.io(s_daylightEverywhere);
    s.io(s_numPlayers); s.io(s_localPlayer);
    uint32_t a = s_rng.s0(), b = s_rng.s1();
    s.io(a); s.io(b);
    if (s.loading()) s_rng.setState(a, b);
    s.io(s_cheat);
    s.io(s_startFlags);
}
void tethysSerialize(SaveStream& s) { TethysGame::serialize(s); }
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
