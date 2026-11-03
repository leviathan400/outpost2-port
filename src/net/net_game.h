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

// Onde fica cada um dos outros, em ordem de rede.
struct NetPeer {
    uint32_t addr = 0;    // IP em ordem de rede
    uint16_t port = 0;    // porta em ordem local
};

// Comeca a partida com o socket e os enderecos que o LOBBY ja negociou
// (lobbyTakeGameSocket / lobbyHostPlayer / lobbyPeer): nao ha aperto de mao a
// refazer, porque o do lobby — pedido 0x32, aceitacao 0x26, confirmacao 0x14 —
// ja colocou todos de acordo sobre quem e quem. `players` conta a mesa inteira.
//
// A TOPOLOGIA e a mesma da camada de mensagens do lobby: ESTRELA com o host no
// meio. O host recebe `peers` com todos os clientes e `relay = true`; cada
// cliente recebe so o host e `relay = false`. Com o repasse ligado, todo turno
// que chega de um cliente e reenviado aos demais, que e o que permite 3 a 6
// jogadores sem exigir que cada um alcance todos os outros (NAT, firewall).
bool netStartJoined(uintptr_t sock, const NetPeer* peers, int numPeers, bool relay,
                    int localPlayer, int players, std::string& error);
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

// O trafego acumulado desde o inicio da partida, que e o que a tela
// "Network Statistics" da aba Communications mostra.
void netTraffic(int& pktSent, int& byteSent, int& pktRecv, int& byteRecv);

} // namespace op2
