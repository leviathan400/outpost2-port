#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../game/command.h"

namespace op2 {

// Partida em rede — porte do lockstep do original.
//
// No original cada jogador humano tem um ANEL DE 16 TURNOS em [_Player+0x4C8]
// (slots de 0x71 bytes, montados por sub_4905E0 e reciclados por sub_490A40).
// Cada slot é um CommandPacket do tipo **0x33** com o cabeçalho de 0x0E bytes:
// +0 tipo, +4 tamanho, **+6 o TICK de destino** e +0x0A uma marca. O índice do
// slot é `(tick >> log2(periodo)) & 0x0F` (sub_490730 / sub_490A40), ou seja os
// comandos de um jogador são anunciados para um turno FUTURO e o jogo só pode
// avançar quando o turno de todos chegou. É esse mecanismo que está portado
// aqui; o transporte é nosso (UDP), porque o original usava Winsock + DirectPlay
// e a descoberta por broadcast de sub_48C1F0.
//
// O período do turno (dword_56DEC8) e o seu log2 (dword_56DECC) são campos do
// objeto do jogo e não aparecem como símbolo no disassembly: aqui é
// kTurnPeriod, e a folga de 16 turnos do anel é a mesma do original.
struct NetConfig {
    bool host = false;          // true = espera o outro; false = conecta
    int port = 47800;
    std::string peerHost;       // só para quem conecta
    int localPlayer = 0;
};

constexpr int kTurnPeriod = 4;      // ticks por turno
constexpr int kTurnRing = 16;       // slots do anel ([_Player+0x4C8], 16 slots)
constexpr int kTurnLead = 2;        // turnos de antecedência do anúncio

// Abre o socket e faz o aperto de mão (quem hospeda espera). false = falhou.
bool netStart(const NetConfig& cfg, std::string& error);
void netStop();
bool netActive();
int  netLocalPlayer();
int  netPlayerCount();

// Comando do JOGADOR LOCAL: em rede vai para o turno futuro em vez de ser
// aplicado na hora; fora de rede é aplicado direto.
void commandSubmit(int owner, const CommandPacket& pkt);

// Chamado antes de avançar o tick. false = ainda falta o turno de alguém (o
// jogo tem de esperar, que é o que mantém as duas máquinas em passo).
bool netReadyForTick(int tick);

// Aplica os comandos cujo turno começa neste tick (de todos os jogadores, na
// ordem do número do jogador, para a ordem ser igual nas duas máquinas).
void netApplyTurn(int tick);

// Diagnóstico: quantos turnos esperamos e por quanto tempo.
int netWaitCount();

} // namespace op2
