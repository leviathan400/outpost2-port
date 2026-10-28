#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Lobby de rede — porte do protocolo de descoberta e entrada do original.
//
// O original tem dois lados:
//
//   HOST  (sub_491BE0 cria a sessao, sub_495E00 abre os sockets):
//     - thread sub_48C1F0 escuta na porta 0xBAA0 e responde ao broadcast de
//       descoberta (consulta de 0x2A bytes, resposta de 0x51);
//     - thread sub_48C300 escuta na porta 0xBAA1 e atende os pedidos de
//       entrada (0x32 bytes), chamando sub_496170 para atribuir o numero do
//       jogador e abrindo um thread sub_48C560 por jogador, que manda a
//       aceitacao (0x26) de uma porta propria e espera a confirmacao (0x14).
//
//   CLIENTE (sub_491410 enumera, sub_491D10/sub_496370/sub_4964D0 entram):
//     - liga um socket na primeira porta livre de 0xBAB8..0xBABF e manda a
//       consulta 0x1000 em broadcast para 255.255.255.255:0xBAA0 (sub_491710),
//       ou para uma lista de enderecos (sub_491860); o thread sub_491980
//       recolhe as respostas 0x1001 e monta a lista de sessoes;
//     - para entrar, manda o pedido de 0x32 ate 16 vezes e, ao receber a
//       aceitacao, confirma com o pacote de 0x14 (sub_4970C0).
//
// Aqui os pacotes e as portas sao os mesmos; o que muda e a estrutura: em vez
// de um thread por socket o lobby e bombeado pelo laco (lobbyHostPump), o que
// dispensa as secoes criticas do original.
//
// Dois defeitos do original NAO sao reproduzidos (ver feedback do usuario:
// corrigir, nao portar): o pedido de entrada (sub_4964D0) e a aceitacao
// (sub_48C560) montam o cabecalho sem zerar os campos +0..+7 e +4..+7, e
// mandam esses bytes de PILHA NAO INICIALIZADA pela rede. Aqui o pacote todo
// nasce zerado.

// As portas do original.
constexpr uint16_t kLobbyDiscoveryPort = 0xBAA0;   // 47776 — consultas
constexpr uint16_t kLobbyJoinPort      = 0xBAA1;   // 47777 — pedidos de entrada
constexpr uint16_t kLobbyClientPortLo  = 0xBAB8;   // 47800 — primeira porta do cliente
constexpr uint16_t kLobbyClientPortHi  = 0xBABF;   // 47807 — ultima
constexpr uint16_t kLobbyAcceptPortLo  = 0xBAA7;   // 47783 — (esi - 0x4559) em sub_48C560

// Os tipos de pacote.
constexpr uint16_t kLobbyQuery = 0x1000;           // consulta de descoberta
constexpr uint16_t kLobbyReply = 0x1001;           // resposta do host

// Uma sessao encontrada pela descoberta.
struct LobbySession {
    uint32_t addr = 0;          // IP do host, em ordem de rede
    uint8_t  guid[16]{};        // GUID da sessao (CoCreateGuid no host)
    std::string name;           // nome da partida (14 bytes no pacote)
    int maxPlayers = 0;         // o campo +0x3A, lido por sub_496170
    uint32_t settingsFlags = 0; // +0x2A: os flags da partida (bit 0 desastres,
                                //   bit 1 dia/noite, bit 2 moral fixa,
                                //   bits 6-8 maximo de jogadores,
                                //   bits 17-20 veiculos iniciais)
    int pingMs = 0;             // soma das idas e voltas
    int replies = 0;            // quantas respostas chegaram
};

// Um lugar na mesa do host. O slot 0 e o proprio host.
struct LobbyPlayer {
    uint32_t addr = 0;          // IP, em ordem de rede
    uint16_t port = 0;          // a porta que o cliente pediu (ordem local)
    uint32_t id = 0;            // (timeGetTime() & ~7) | numero, como sub_496170
    int state = 0;              // 0 livre, 1 esperando confirmacao, 2 ativo
};

// ---- host ------------------------------------------------------------------

// Abre as portas 0xBAA0/0xBAA1 e passa a anunciar a partida.
bool lobbyHostStart(const std::string& name, int maxPlayers,
                    const std::string& password, std::string& error,
                    uint32_t settingsFlags = 0);
void lobbyHostStop();
bool lobbyHostActive();

// Atende o que chegou: responde consultas, aceita entradas, recebe
// confirmacoes. Nao bloqueia — e para ser chamado do laco do lobby.
void lobbyHostPump();

// Quantos lugares estao ativos (contando o host) e o conteudo de cada um.
int lobbyHostActiveCount();
int lobbyHostMaxPlayers();
const LobbyPlayer* lobbyHostPlayer(int n);

// ---- cliente ---------------------------------------------------------------

// Manda a consulta em broadcast `attempts` vezes (o original manda 8, com
// 250 ms entre elas) e devolve as sessoes que responderam.
bool lobbyFind(const std::string& password, int attempts,
               std::vector<LobbySession>& out, std::string& error);

// Pede entrada na sessao. Em caso de aceitacao devolve o numero do jogador
// (os 3 bits baixos do id, como no original) e a porta de jogo do host.
// `error` traz "sessao cheia" quando o host responde com o codigo 2.
bool lobbyJoin(const LobbySession& s, const std::string& password,
               int& playerNumber, uint16_t& hostGamePort, std::string& error);

// A porta local que o cliente ficou escutando (0xBAB8..0xBABF), valida depois
// de um lobbyJoin bem-sucedido: e a porta de jogo deste lado.
uint16_t lobbyLocalGamePort();

// ---- a ponte para a partida ------------------------------------------------

// O socket de jogo que o lobby deixou aberto: o do host esta preso a 0xBAB8
// (sub_495E00 liga nessa porta exata), o do cliente a porta que sobrou. E este
// socket que a partida usa, para nao reabrir o que ja esta negociado.
// Depois de entregue com lobbyTakeGameSocket, fechar passa a ser de quem o
// recebeu, e o lobby nao mexe mais nele.
uintptr_t lobbyGameSocket();
uintptr_t lobbyTakeGameSocket();

// Onde fica o outro lado, para quem so tem dois na mesa: o cliente ve o host,
// o host ve o primeiro lugar ocupado. false = ninguem ainda.
bool lobbyPeer(uint32_t& addr, uint16_t& port);

// ---- somas de verificacao (expostas para teste) ----------------------------

// sub_490DE0: soma os dwords e, se sobrar resto, soma-o e aplica o XOR.
uint32_t lobbySum(const uint8_t* p, int len);
// sub_490E40: a soma de um pacote com cabecalho de 0x0E bytes, cujo tamanho de
// corpo esta no byte +8.
uint32_t lobbyPacketSum(const uint8_t* p);

} // namespace op2
