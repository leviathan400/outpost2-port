#pragma once

#include <cstdint>

namespace op2 {

// RNG determinístico do Outpost 2 — portado 1:1 do disassembly (objeto em
// unk_56B1E0; SetSeed=sub_46EEA0, next=sub_46EEF0, GetRand=sub_46EF50).
// É um par de LFSRs de Fibonacci de 32 bits (taps nos bits 13 e 16), um
// deslocando à esquerda e outro à direita, combinados por XOR. Determinismo é
// essencial (replays/multiplayer), então o algoritmo tem de bater bit a bit.
class GameRNG {
public:
    // SetSeed (sub_46EEA0): seed 0 vira 0x10000001; espalha os words do seed.
    void setSeed(uint32_t seed);

    // next (sub_46EEF0): avança os dois LFSRs e retorna s0^s1 (32 bits crus).
    uint32_t next();

    // GetRand (sub_46EF50): inteiro em [0, n) = (uint64(next()) * n) >> 32.
    int getRand(int n);

    uint32_t s0() const { return m_s0; }
    uint32_t s1() const { return m_s1; }

private:
    uint32_t m_s0 = 0;
    uint32_t m_s1 = 0;
};

} // namespace op2
