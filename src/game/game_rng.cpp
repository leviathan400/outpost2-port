#include "game_rng.h"

namespace op2 {

void GameRNG::setSeed(uint32_t seed) {
    if (seed == 0) seed = 0x10000001u;          // evita estado todo-zero
    uint32_t hi = (seed >> 16) & 0xFFFFu;        // word alto
    uint32_t lo = seed & 0xFFFFu;                // word baixo
    m_s0 = (hi << 16) | hi;                       // s0 = word alto replicado
    m_s1 = (lo << 16) | lo;                       // s1 = word baixo replicado
}

uint32_t GameRNG::next() {
    uint32_t a = m_s0;
    uint32_t b = m_s1;
    // LFSR à esquerda: realimenta o bit 0 com (bit13 XOR bit16) do valor atual.
    uint32_t fb0 = ((a >> 13) ^ (a >> 16)) & 1u;
    a = (a << 1) | fb0;
    // LFSR à direita: realimenta o bit 31 com (bit13 XOR bit16) do valor atual.
    uint32_t fb1 = ((b >> 13) ^ (b >> 16)) & 1u;
    b = (b >> 1) | (fb1 << 31);
    m_s0 = a;
    m_s1 = b;
    return a ^ b;
}

int GameRNG::getRand(int n) {
    uint64_t prod = static_cast<uint64_t>(next()) * static_cast<uint32_t>(n);
    return static_cast<int>(prod >> 32);
}

} // namespace op2
