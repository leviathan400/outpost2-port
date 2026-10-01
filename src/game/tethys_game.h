#pragma once

#include <cstdint>

#include "game_rng.h"
#include "unit.h"
#include "location.h"

namespace op2 {

// Núcleo da simulação — porta TethysGame (asm @193905+), que no original é uma
// classe de métodos static sobre globais. Ver [[op2-engine-architecture]].
//   Tick()  = dword_56DEDC            (contador de ticks de simulação)
//   Time()  = dword_56DEDC >> 2       (4 ticks por "marca")
//   NoPlayers()/LocalPlayer()         = dword_56DEC0 / dword_56DEE8
//   GetRand/SetSeed                   = RNG determinístico (unk_56B1E0)
class TethysGame {
public:
    static void reset(uint32_t seed, int numPlayers, int localPlayer);

    static int tick()        { return static_cast<int>(s_tick); }
    static int time()        { return static_cast<int>(s_tick >> 2); }
    static int noPlayers()   { return s_numPlayers; }
    static int localPlayer() { return s_localPlayer; }

    // Avança um tick de simulação (o jogo incrementa dword_56DEDC por passo).
    static void advanceTick() { ++s_tick; }

    static void setSeed(uint32_t seed) { s_rng.setSeed(seed); }
    static int  getRand(int n)         { return s_rng.getRand(n); }
    static GameRNG& rng()              { return s_rng; }

    // CreateUnit (@195141): cria uma unidade e devolve o handle em `unit`.
    static int CreateUnit(Unit& unit, map_id type, LOCATION loc, int owner,
                          map_id cargo = kMapIdNone, int rotation = 0) {
        return createUnit(unit, type, loc, owner, cargo, rotation) ? 1 : 0;
    }

private:
    static uint32_t s_tick;        // dword_56DEDC
    static int      s_numPlayers;  // dword_56DEC0
    static int      s_localPlayer; // dword_56DEE8
    static GameRNG  s_rng;         // unk_56B1E0
};

} // namespace op2
