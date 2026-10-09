#pragma once

#include <cstdint>
#include <vector>

#include "location.h"

namespace op2 {

// Sistema de comando do engine (porta a cadeia CommandPacket → sub_40E210).
// No original todo comando (jogador/IA/rede) é serializado num CommandPacket
// (header 0x0E + dados) e despachado por sub_40E210 sobre o objeto de comando
// do jogador (dword_56E2DC + owner*777*4). Aqui o packet é uma struct com os
// MESMOS campos (o formato de bits do LOCATION é idêntico ao do asm); a
// serialização em bytes de rede/replay (sub_490840/490880/410210) é um detalhe
// de transporte, fora do mecanismo de movimento — documentado, não portado ainda.

// --- Empacotamento de LOCATION (idêntico a Unit::DoMove @190538) -------------
// loc = (x<<5 & 0x7FFF) | (y<<20 & 0x1FFF8000), campo de 29 bits (& 0x1FFFFFFF).
// (x ocupa bits 5..14, y ocupa bits 20..28; iguais ao cell+0x10 do grid.)
inline uint32_t packLocation(int x, int y) {
    uint32_t v = (static_cast<uint32_t>(x) << 5) & 0x7FFFu;
    v = (((static_cast<uint32_t>(y) << 20) ^ v) & 0x1FFF8000u) ^ v;
    return v & 0x1FFFFFFFu;
}
inline int unpackLocX(uint32_t loc) { return static_cast<int>((loc & 0x7FE0u) >> 5); }
inline int unpackLocY(uint32_t loc) { return static_cast<int>((loc & 0x1FF00000u) >> 20); }

// Tipos de comando (byte [packet+0]) — subconjunto; MOVE = 2 (sub_40E210 agrupa
// os casos 2-4 e 38-40 no mesmo caminho de ordem de movimento).
enum CommandType : int {
    CMD_MOVE = 2,
};

// CommandPacket — header 0x0E + dados. Campos equivalentes ao layout do asm:
//   [+0]=tipo, [+4]=tamanho; dados: [count][ids...][waypoints...].
struct CommandPacket {
    int type = 0;                      // [packet+0]
    std::vector<int> unitIds;          // lista de unidades-alvo do comando
    std::vector<uint32_t> waypoints;   // LOCATIONs empacotados (destino/rota)
    int patrolStart = 0;               // 0x28: 1ª palavra após os pontos → unidade+0x5E
    // 0x27 (rota de minério): 4 palavras após os pontos → +0x62/+0x64/+0x66/+0x68.
    int routeMineIdx = 0, routeSmelterIdx = 0, routeMine = 0, routeSmelter = 0;
};

// Scratch global da busca em espiral (porta dword_5472E8/EC/F0/F4/F8) — setado por
// sub_4230F0 p/ a 1ª unidade do grupo e avançado p/ cada unidade seguinte, que
// recebe a próxima célula enterável da espiral (espalha o grupo ao redor do alvo).
struct PathScratch {
    int leg     = 1;   // dword_5472E8 — comprimento da perna atual (cresce)
    int state   = 0;   // dword_5472EC — direção 0..3 (E, N, O, S)
    int steps   = 1;   // dword_5472F0 — passos restantes na perna
    int x       = 0;   // dword_5472F4 — x atual da espiral
    int y       = 0;   // dword_5472F8 — y atual da espiral
};
extern PathScratch g_pathScratch;

// Despacha um CommandPacket pelo objeto de comando do jogador `owner`
// (porta sub_40E210). Implementa o caso MOVE (loc_40E2C0): para cada unidade
// cujo dono confere e que é comandável, reseta e seta a ordem de movimento.
void processCommandPacket(int owner, const CommandPacket& pkt);

} // namespace op2
