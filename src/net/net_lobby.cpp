#include "net_lobby.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

#include <cstring>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")

namespace op2 {

namespace {

// O GUID da aplicacao do Outpost 2, tirado de unk_4E9AE0 — e o arg_0 que
// sub_40DEC0 (CREATE no TCP/IP) passa a sub_491BE0 e que o responder compara
// em +6 da consulta. Em forma de GUID: {5A55CF11-B841-11CE-9210-00AA006C4972};
// no pacote vai nesta ordem de bytes, que e a da memoria.
const uint8_t kAppGuid[16] = {
    0x11, 0xCF, 0x55, 0x5A, 0x41, 0xB8, 0xCE, 0x11,
    0x92, 0x10, 0x00, 0xAA, 0x00, 0x6C, 0x49, 0x72,
};

// O bloco de 16 bytes de +0x2A da resposta e o arg_10 de sub_491BE0: no
// original e o inicio da estrutura de configuracao da partida (o maximo de
// jogadores sai de `(flags & 0x1C0) >> 6` dela). Preenchido em lobbyHostStart.

bool s_wsa = false;
int  s_wsaRefs = 0;

// ---- host ------------------------------------------------------------------
bool   s_hostActive = false;
SOCKET s_discSock  = INVALID_SOCKET;   // 0xBAA0, as consultas
SOCKET s_joinSock  = INVALID_SOCKET;   // 0xBAA1, os pedidos de entrada
SOCKET s_replySock = INVALID_SOCKET;   // de onde saem as respostas de descoberta
SOCKET s_acceptSock[8];                // um por lugar, como sub_48C560
uint32_t s_acceptSent[8];              // quando a ultima aceitacao saiu
int      s_acceptTries[8];
uint8_t  s_replyPkt[0x51];             // o buffer de sub_491BE0
uint32_t s_settingsFlags = 0;          // os flags da partida (arg_10)
uint32_t s_sessionId = 0;              // timeGetTime() & ~7
int      s_maxPlayers = 0;
int      s_activeCount = 0;
std::string s_password;
LobbyPlayer s_slots[8];

SOCKET s_gameSock = INVALID_SOCKET;     // a porta de jogo deste lado
bool   s_gameSockTaken = false;

// ---- cliente ---------------------------------------------------------------
uint16_t s_localGamePort = 0;
uint32_t s_peerAddr = 0;
uint16_t s_peerPort = 0;

// ---- utilitarios -----------------------------------------------------------
void putU16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
void putU32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v); p[1] = uint8_t(v >> 8);
    p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24);
}
uint16_t getU16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t getU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

bool wsaUp(std::string& error) {
    if (!s_wsa) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { error = "WSAStartup falhou"; return false; }
        s_wsa = true;
    }
    ++s_wsaRefs;
    return true;
}
void wsaDown() {
    if (--s_wsaRefs <= 0 && s_wsa) { WSACleanup(); s_wsa = false; s_wsaRefs = 0; }
}

void setNonBlocking(SOCKET s) { u_long nb = 1; ioctlsocket(s, FIONBIO, &nb); }

// O host compara 11 bytes, sem distinguir maiusculas (strnicmp em sub_48C1F0 e
// sub_48C300). Quando nao ha senha o campo nem e olhado.
bool passwordMatches(const std::string& want, const uint8_t* field) {
    if (want.empty()) return true;
    char got[12];
    std::memcpy(got, field, 11);
    got[11] = 0;
    return _strnicmp(want.c_str(), got, 11) == 0;
}

void writePassword(uint8_t* field, const std::string& pw) {
    std::memset(field, 0, 12);
    if (!pw.empty()) std::memcpy(field, pw.c_str(), pw.size() < 11 ? pw.size() : 11);
}

// O IP desta maquina, como o original o obtem: gethostbyname("localhost") para
// descobrir o nome, depois gethostbyname(nome) para o endereco.
uint32_t localAddr() {
    hostent* h = gethostbyname("localhost");
    if (!h) return htonl(INADDR_LOOPBACK);
    h = gethostbyname(h->h_name);
    if (!h || !h->h_addr_list || !h->h_addr_list[0]) return htonl(INADDR_LOOPBACK);
    uint32_t a = 0;
    std::memcpy(&a, h->h_addr_list[0], 4);
    return a;
}

void closeSock(SOCKET& s) {
    if (s != INVALID_SOCKET) { closesocket(s); s = INVALID_SOCKET; }
}

} // namespace

// ---- somas de verificacao --------------------------------------------------

// sub_490DE0.
uint32_t lobbySum(const uint8_t* p, int len) {
    uint32_t sum = 0;
    const int words = len / 4;
    for (int i = 0; i < words; ++i) sum += getU32(p + i * 4);
    const int rem = len & 3;
    if (rem) {
        uint32_t tail = 0;
        std::memcpy(&tail, p + words * 4, size_t(rem));   // num dword zerado
        sum += tail;
        sum ^= 0xFDE24ACBu;
    }
    return sum;
}

// sub_490E40: o cabecalho (os dois dwords de +0 e o word de +8) numa parcela, o
// corpo — cujo tamanho esta no byte +8 — na outra. O proprio campo da soma, em
// +0x0A, fica de fora das duas.
uint32_t lobbyPacketSum(const uint8_t* p) {
    uint32_t head = getU32(p) + getU32(p + 4) + getU16(p + 8);
    head ^= 0xFDE24ACBu;
    return head + lobbySum(p + 0x0E, p[8]);
}

// ---- host ------------------------------------------------------------------

bool lobbyHostStart(const std::string& name, int maxPlayers,
                    const std::string& password, std::string& error,
                    uint32_t settingsFlags) {
    s_settingsFlags = settingsFlags;
    if (s_hostActive) { error = "o lobby ja esta no ar"; return false; }
    if (!wsaUp(error)) return false;
    for (int i = 0; i < 8; ++i) {
        s_acceptSock[i] = INVALID_SOCKET;
        s_acceptSent[i] = 0;
        s_acceptTries[i] = 0;
        s_slots[i] = LobbyPlayer{};
    }

    s_discSock  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    s_joinSock  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    s_replySock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_discSock == INVALID_SOCKET || s_joinSock == INVALID_SOCKET ||
        s_replySock == INVALID_SOCKET) {
        error = "socket falhou"; lobbyHostStop(); return false;
    }

    const uint32_t me = localAddr();
    sockaddr_in a{};
    a.sin_family = AF_INET;
    // CORRIGIDO: o original (sub_48BFB0) liga as duas portas ao IP da
    // interface. Um socket preso a um endereco unicast nao recebe o que foi
    // mandado para 255.255.255.255, que e justamente como a consulta viaja
    // (sub_491710), entao a descoberta so funcionava onde a pilha era
    // tolerante. Aqui as duas portas escutam em INADDR_ANY.
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(kLobbyDiscoveryPort);
    if (bind(s_discSock, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        error = "a porta de descoberta ja esta ocupada"; lobbyHostStop(); return false;
    }
    a.sin_port = htons(kLobbyJoinPort);
    if (bind(s_joinSock, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
        error = "a porta de entrada ja esta ocupada"; lobbyHostStop(); return false;
    }
    setNonBlocking(s_discSock);
    setNonBlocking(s_joinSock);

    // A porta de jogo. sub_495E00 liga o socket do host na 0xBAB8 exata (e o
    // cliente, em sub_496370, pega a primeira livre depois dela) — sem essa
    // reserva os dois lados na mesma maquina acabam na mesma porta.
    s_gameSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    s_gameSockTaken = false;
    sockaddr_in gameAddr{};
    gameAddr.sin_family = AF_INET;
    // Esta, ao contrario das de descoberta, vai presa ao IP da interface, como
    // no original: so assim a porta fica de fato tomada e o cliente da mesma
    // maquina e empurrado para a 0xBAB9 em diante.
    gameAddr.sin_addr.s_addr = me;
    gameAddr.sin_port = htons(kLobbyClientPortLo);
    if (s_gameSock == INVALID_SOCKET ||
        bind(s_gameSock, reinterpret_cast<sockaddr*>(&gameAddr), sizeof(gameAddr)) != 0) {
        error = "a porta de jogo 47800 ja esta ocupada"; lobbyHostStop(); return false;
    }
    setNonBlocking(s_gameSock);

    // O buffer de 0x51 bytes que sub_491BE0 monta e o responder so retoca.
    std::memset(s_replyPkt, 0, sizeof(s_replyPkt));
    putU16(s_replyPkt + 0x04, kLobbyReply);
    std::memcpy(s_replyPkt + 0x0A, kAppGuid, 16);
    GUID g{};
    CoCreateGuid(&g);
    std::memcpy(s_replyPkt + 0x1A, &g, 16);
    putU32(s_replyPkt + 0x2A, s_settingsFlags);   // a config da partida (arg_10)
    putU32(s_replyPkt + 0x3A, uint32_t(maxPlayers));
    std::strncpy(reinterpret_cast<char*>(s_replyPkt) + 0x42, name.c_str(), 14);

    // O id da sessao: sub_495E00 o tira do relogio, com os 3 bits baixos
    // livres para o numero do jogador (sub_496170 faz `id | n`).
    s_sessionId = GetTickCount() & ~7u;
    s_maxPlayers = maxPlayers;
    s_password = password;

    // O lugar 0 e o host, com a porta de jogo 0xBAB8.
    s_slots[0].addr = me;
    s_slots[0].port = kLobbyClientPortLo;
    s_slots[0].id = s_sessionId;
    s_slots[0].state = 2;
    s_activeCount = 1;

    s_hostActive = true;
    return true;
}

void lobbyHostStop() {
    closeSock(s_discSock);
    closeSock(s_joinSock);
    closeSock(s_replySock);
    if (!s_gameSockTaken) closeSock(s_gameSock); else s_gameSock = INVALID_SOCKET;
    for (int i = 0; i < 8; ++i) closeSock(s_acceptSock[i]);
    if (s_hostActive || s_wsaRefs > 0) wsaDown();
    s_hostActive = false;
    s_activeCount = 0;
}

bool lobbyHostActive() { return s_hostActive; }
int  lobbyHostActiveCount() { return s_activeCount; }
int  lobbyHostMaxPlayers() { return s_maxPlayers; }

const LobbyPlayer* lobbyHostPlayer(int n) {
    if (n < 0 || n >= 8) return nullptr;
    return &s_slots[n];
}

namespace {

// sub_48C1F0: responde a uma consulta de descoberta.
void hostAnswerQueries() {
    uint8_t buf[0x2A];
    for (;;) {
        sockaddr_in from{};
        int flen = sizeof(from);
        const int n = recvfrom(s_discSock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                               reinterpret_cast<sockaddr*>(&from), &flen);
        if (n < 0) return;                       // a fila secou
        if (n < int(sizeof(buf))) continue;      // curto demais: o proximo
        if (lobbySum(buf + 4, 0x26) != getU32(buf)) continue;
        if (getU16(buf + 4) != kLobbyQuery) continue;
        if (!passwordMatches(s_password, buf + 0x1E)) continue;
        if (std::memcmp(buf + 6, s_replyPkt + 0x0A, 16) != 0) continue;

        // Ecoa o relogio do cliente (ele mede a ida e volta com isso) e fecha
        // a soma antes de mandar.
        std::memcpy(s_replyPkt + 6, buf + 0x1A, 4);
        putU32(s_replyPkt, lobbySum(s_replyPkt + 4, 0x4D));

        sockaddr_in to = from;
        to.sin_port = htons(getU16(buf + 0x16));
        sendto(s_replySock, reinterpret_cast<const char*>(s_replyPkt), sizeof(s_replyPkt), 0,
               reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    }
}

// sub_496170: o primeiro lugar livre, com o numero nos 3 bits baixos do id.
int hostAssignSlot() {
    if (s_maxPlayers <= s_activeCount) return 0;
    int n = 1;
    while (n < 8 && s_slots[n].id != 0) ++n;
    if (n >= 8) return 0;
    ++s_activeCount;
    s_slots[n].id = (GetTickCount() & ~7u) | uint32_t(n);
    s_slots[n].state = 1;
    return n;
}

// O pacote de 0x26 bytes de sub_48C560 (aceitacao) e de sub_48C300 (recusa).
void buildJoinReply(uint8_t* pkt, uint32_t code, uint32_t playerId) {
    std::memset(pkt, 0, 0x26);
    putU32(pkt, s_sessionId);
    pkt[8] = 0x18;                      // tamanho do corpo
    pkt[9] = 1;                         // tipo
    putU32(pkt + 0x0E, code);           // 1 = aceito, 2 = sessao cheia
    std::memcpy(pkt + 0x12, s_replyPkt + 0x1A, 16);
    putU32(pkt + 0x22, playerId);
    putU32(pkt + 0x0A, lobbyPacketSum(pkt));
}

void hostSendAccept(int n) {
    uint8_t pkt[0x26];
    buildJoinReply(pkt, 1, s_slots[n].id);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = s_slots[n].addr;
    to.sin_port = htons(s_slots[n].port);
    sendto(s_acceptSock[n], reinterpret_cast<const char*>(pkt), sizeof(pkt), 0,
           reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    s_acceptSent[n] = GetTickCount();
    ++s_acceptTries[n];
}

// sub_48C300: atende um pedido de entrada.
void hostAcceptJoins() {
    uint8_t buf[0x32];
    for (;;) {
        sockaddr_in from{};
        int flen = sizeof(from);
        const int n = recvfrom(s_joinSock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                               reinterpret_cast<sockaddr*>(&from), &flen);
        if (n < 0) return;
        if (n < int(sizeof(buf))) continue;
        if (lobbyPacketSum(buf) != getU32(buf + 0x0A)) continue;
        if (!passwordMatches(s_password, buf + 0x26)) continue;
        if (buf[9] != 1 || getU32(buf + 0x0E) != 0) continue;
        if (std::memcmp(buf + 0x12, s_replyPkt + 0x1A, 16) != 0) continue;

        const uint32_t addr = from.sin_addr.s_addr;
        const uint16_t port = uint16_t(getU32(buf + 0x22));

        // Ja esta na mesa? O original sai calado, e o reenvio do cliente so
        // volta a cair no thread que ja o atende.
        bool known = false;
        for (int p = 1; p < 8; ++p)
            if (s_slots[p].id != 0 && s_slots[p].addr == addr && s_slots[p].port == port)
                known = true;
        if (known) continue;

        const int slot = hostAssignSlot();
        if (slot == 0) {
            // Sessao cheia: a recusa sai pela propria porta de entrada.
            uint8_t rej[0x26];
            buildJoinReply(rej, 2, 0);
            sockaddr_in to = from;
            to.sin_port = htons(port);
            sendto(s_joinSock, reinterpret_cast<const char*>(rej), sizeof(rej), 0,
                   reinterpret_cast<const sockaddr*>(&to), sizeof(to));
            continue;
        }

        s_slots[slot].addr = addr;
        s_slots[slot].port = port;

        // Cada lugar ganha um socket proprio, na porta 0xBAA7 + (slot - 1), de
        // onde sai a aceitacao e por onde volta a confirmacao.
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in me{};
        me.sin_family = AF_INET;
        me.sin_addr.s_addr = s_slots[0].addr;
        me.sin_port = htons(uint16_t(kLobbyAcceptPortLo + slot - 1));
        if (s == INVALID_SOCKET ||
            bind(s, reinterpret_cast<sockaddr*>(&me), sizeof(me)) != 0) {
            if (s != INVALID_SOCKET) closesocket(s);
            s_slots[slot] = LobbyPlayer{};
            --s_activeCount;
            continue;
        }
        setNonBlocking(s);
        closeSock(s_acceptSock[slot]);
        s_acceptSock[slot] = s;
        s_acceptTries[slot] = 0;
        hostSendAccept(slot);
    }
}

// A confirmacao de 0x14 bytes que sub_4970C0 manda e sub_48C560 espera.
void hostCollectConfirms() {
    for (int n = 1; n < 8; ++n) {
        if (s_acceptSock[n] == INVALID_SOCKET || s_slots[n].state != 1) continue;
        uint8_t buf[0x14];
        for (;;) {
            sockaddr_in from{};
            int flen = sizeof(from);
            const int got = recvfrom(s_acceptSock[n], reinterpret_cast<char*>(buf), sizeof(buf), 0,
                                     reinterpret_cast<sockaddr*>(&from), &flen);
            if (got < 0) break;
            if (got < int(sizeof(buf))) continue;
            if (buf[8] != 6 || buf[9] != 1) continue;
            if (lobbyPacketSum(buf) != getU32(buf + 0x0A)) continue;
            if (getU32(buf + 0x0E) != 6 || getU16(buf + 0x12) != 2) continue;
            if (getU32(buf + 4) != s_sessionId) continue;
            if (getU32(buf) != s_slots[n].id) continue;
            s_slots[n].state = 2;            // sub_496250
            closeSock(s_acceptSock[n]);
            break;
        }
        // Enquanto nao confirma, repete a aceitacao — ate 16 vezes, como o
        // laco de sub_48C560.
        if (s_slots[n].state == 1 && s_acceptSock[n] != INVALID_SOCKET) {
            if (s_acceptTries[n] >= 16) {
                closeSock(s_acceptSock[n]);
                s_slots[n] = LobbyPlayer{};
                --s_activeCount;
            } else if (GetTickCount() - s_acceptSent[n] >= 250) {
                hostSendAccept(n);
            }
        }
    }
}

} // namespace

void lobbyHostPump() {
    if (!s_hostActive) return;
    hostAnswerQueries();
    hostAcceptJoins();
    hostCollectConfirms();
}

// ---- cliente ---------------------------------------------------------------

uint16_t lobbyLocalGamePort() { return s_localGamePort; }

uintptr_t lobbyGameSocket() { return uintptr_t(s_gameSock); }

uintptr_t lobbyTakeGameSocket() {
    s_gameSockTaken = true;
    // O socket passa a viver mais que o lobby, entao segura o Winsock junto:
    // sem isso o WSACleanup de lobbyHostStop derruba o socket recem-entregue e
    // a partida comeca com um descritor morto.
    std::string ignored;
    wsaUp(ignored);
    return uintptr_t(s_gameSock);
}

bool lobbyPeer(uint32_t& addr, uint16_t& port) {
    if (s_hostActive) {
        for (int n = 1; n < 8; ++n) {
            if (s_slots[n].state != 2) continue;
            addr = s_slots[n].addr;
            port = s_slots[n].port;
            return true;
        }
        return false;
    }
    if (s_peerAddr == 0) return false;
    addr = s_peerAddr;
    port = s_peerPort;
    return true;
}

namespace {

// Liga um socket na primeira porta livre de 0xBAB8..0xBABF, como sub_491410 e
// sub_496370. Devolve a porta, ou 0.
uint16_t bindClientPort(SOCKET s, uint32_t addr) {
    for (uint16_t p = kLobbyClientPortLo; p <= kLobbyClientPortHi; ++p) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = addr;
        a.sin_port = htons(p);
        if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0) return p;
    }
    return 0;
}

} // namespace

bool lobbyFind(const std::string& password, int attempts,
               std::vector<LobbySession>& out, std::string& error) {
    out.clear();
    if (!wsaUp(error)) return false;

    const uint32_t me = localAddr();
    SOCKET rx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET tx = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rx == INVALID_SOCKET || tx == INVALID_SOCKET) {
        error = "socket falhou"; closeSock(rx); closeSock(tx); wsaDown(); return false;
    }
    const uint16_t myPort = bindClientPort(rx, me);
    if (myPort == 0) {
        error = "nenhuma porta de 47800 a 47807 esta livre";
        closeSock(rx); closeSock(tx); wsaDown(); return false;
    }
    setNonBlocking(rx);
    int on = 1;
    setsockopt(tx, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));

    uint8_t q[0x2A];
    std::memset(q, 0, sizeof(q));
    putU16(q + 0x04, kLobbyQuery);
    std::memcpy(q + 0x06, kAppGuid, 16);
    putU32(q + 0x16, myPort);
    writePassword(q + 0x1E, password);

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = INADDR_BROADCAST;
    to.sin_port = htons(kLobbyDiscoveryPort);

    for (int t = 0; t < attempts; ++t) {
        const uint32_t now = GetTickCount();
        putU32(q + 0x1A, now);
        putU32(q, lobbySum(q + 4, 0x26));
        sendto(tx, reinterpret_cast<const char*>(q), sizeof(q), 0,
               reinterpret_cast<const sockaddr*>(&to), sizeof(to));

        // O original espera 250 ms entre as rajadas, com um thread recolhendo
        // as respostas; aqui o mesmo intervalo e gasto lendo.
        const uint32_t until = GetTickCount() + 250;
        for (;;) {
            uint8_t r[0x51];
            sockaddr_in from{};
            int flen = sizeof(from);
            const int n = recvfrom(rx, reinterpret_cast<char*>(r), sizeof(r), 0,
                                   reinterpret_cast<sockaddr*>(&from), &flen);
            if (n == int(sizeof(r))) {
                if (lobbySum(r + 4, 0x4D) == getU32(r) &&
                    getU16(r + 4) == kLobbyReply &&
                    std::memcmp(r + 0x0A, kAppGuid, 16) == 0) {
                    const int rtt = int(GetTickCount() - getU32(r + 6));
                    LobbySession* found = nullptr;
                    for (LobbySession& s : out)
                        if (std::memcmp(s.guid, r + 0x1A, 16) == 0) found = &s;
                    if (found) {
                        found->pingMs += rtt;
                        ++found->replies;
                    } else {
                        LobbySession s;
                        s.addr = from.sin_addr.s_addr;
                        std::memcpy(s.guid, r + 0x1A, 16);
                        char nm[15];
                        std::memcpy(nm, r + 0x42, 14);
                        nm[14] = 0;
                        s.name = nm;
                        s.maxPlayers = int(getU32(r + 0x3A));
                        s.settingsFlags = getU32(r + 0x2A);
                        s.pingMs = rtt;
                        s.replies = 1;
                        out.push_back(s);
                    }
                }
                continue;                       // pode haver mais na fila
            }
            if (int(GetTickCount() - until) >= 0) break;
            Sleep(5);
        }
    }

    closeSock(rx);
    closeSock(tx);
    wsaDown();
    return true;
}

bool lobbyJoin(const LobbySession& s, const std::string& password,
               int& playerNumber, uint16_t& hostGamePort, std::string& error) {
    playerNumber = -1;
    hostGamePort = 0;
    if (!wsaUp(error)) return false;

    const uint32_t me = localAddr();
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) { error = "socket falhou"; wsaDown(); return false; }
    const uint16_t myPort = bindClientPort(sock, me);
    if (myPort == 0) {
        error = "nenhuma porta de 47800 a 47807 esta livre";
        closeSock(sock); wsaDown(); return false;
    }
    setNonBlocking(sock);
    s_localGamePort = myPort;
    s_peerAddr = s.addr;
    s_peerPort = kLobbyClientPortLo;        // o host joga na 0xBAB8

    // O pedido de 0x32 bytes de sub_4964D0, com o cabecalho zerado (o original
    // manda os 8 primeiros bytes como vierem da pilha).
    uint8_t req[0x32];
    std::memset(req, 0, sizeof(req));
    req[8] = 0x24;                              // tamanho do corpo
    req[9] = 1;                                 // tipo
    putU32(req + 0x0E, 0);                      // codigo 0 = pedido
    std::memcpy(req + 0x12, s.guid, 16);
    putU32(req + 0x22, myPort);
    writePassword(req + 0x26, password);
    putU32(req + 0x0A, lobbyPacketSum(req));

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = s.addr;
    to.sin_port = htons(kLobbyJoinPort);

    for (int t = 0; t < 16; ++t) {
        sendto(sock, reinterpret_cast<const char*>(req), sizeof(req), 0,
               reinterpret_cast<const sockaddr*>(&to), sizeof(to));

        const uint32_t until = GetTickCount() + 250;
        for (;;) {
            uint8_t r[0x26];
            sockaddr_in from{};
            int flen = sizeof(from);
            const int n = recvfrom(sock, reinterpret_cast<char*>(r), sizeof(r), 0,
                                   reinterpret_cast<sockaddr*>(&from), &flen);
            if (n == int(sizeof(r)) && r[8] == 0x18 && r[9] == 1 &&
                lobbyPacketSum(r) == getU32(r + 0x0A) &&
                std::memcmp(r + 0x12, s.guid, 16) == 0) {
                const uint32_t code = getU32(r + 0x0E);
                if (code == 2) {
                    error = "sessao cheia";
                    closeSock(sock); wsaDown(); return false;
                }
                if (code == 1) {
                    const uint32_t id = getU32(r + 0x22);
                    playerNumber = int(id & 7);
                    hostGamePort = kLobbyClientPortLo;   // o host joga em 0xBAB8

                    // A confirmacao de sub_4970C0, de volta para a porta de
                    // onde veio a aceitacao.
                    uint8_t ack[0x14];
                    std::memset(ack, 0, sizeof(ack));
                    putU32(ack, id);
                    putU32(ack + 4, getU32(r));          // o id da sessao
                    ack[8] = 6;
                    ack[9] = 1;
                    putU32(ack + 0x0E, 6);
                    putU16(ack + 0x12, 2);
                    putU32(ack + 0x0A, lobbyPacketSum(ack));
                    sockaddr_in back = from;
                    for (int k = 0; k < 4; ++k)
                        sendto(sock, reinterpret_cast<const char*>(ack), sizeof(ack), 0,
                               reinterpret_cast<const sockaddr*>(&back), sizeof(back));
                    // O socket fica de pe: e a porta de jogo deste lado, e e
                    // dela que a partida vai falar.
                    if (!s_gameSockTaken) closeSock(s_gameSock);
                    s_gameSock = sock;
                    s_gameSockTaken = false;
                    return true;
                }
            }
            if (int(GetTickCount() - until) >= 0) break;
            Sleep(5);
        }
    }

    error = "o host nao respondeu";
    closeSock(sock);
    wsaDown();
    return false;
}

} // namespace op2
