#include "net_game.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>

#include "../game/tethys_game.h"

#pragma comment(lib, "ws2_32.lib")

namespace op2 {

namespace {

constexpr uint32_t kMagic = 0x324F5054u;   // "TPO2"
constexpr int kPktTurn = 0x33;             // o tipo do pacote de turno do original
constexpr int kPktHello = 0x01;

bool s_active = false;
SOCKET s_sock = INVALID_SOCKET;
sockaddr_in s_peer{};
int s_localPlayer = 0;
int s_players = 2;
int s_waits = 0;

// Comandos que o jogador local ainda não anunciou.
std::vector<CommandPacket> s_pending;
// Turnos recebidos (e os nossos): (tick do turno, jogador) -> comandos.
std::map<std::pair<int, int>, std::vector<CommandPacket>> s_turns;
// Turnos que já anunciamos, para reenviar se o outro lado não recebeu.
std::map<int, std::vector<uint8_t>> s_sentTurns;
int s_lastAnnounced = -1;

// ---- serialização ----------------------------------------------------------
void putU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v >> 16)); b.push_back(uint8_t(v >> 24));
}
uint32_t getU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void putCommand(std::vector<uint8_t>& b, const CommandPacket& c) {
    putU32(b, uint32_t(c.type));
    putU32(b, uint32_t(c.unitIds.size()));
    for (int id : c.unitIds) putU32(b, uint32_t(id));
    putU32(b, uint32_t(c.waypoints.size()));
    for (uint32_t w : c.waypoints) putU32(b, w);
    putU32(b, uint32_t(c.patrolStart));
    putU32(b, uint32_t(c.routeMineIdx));
    putU32(b, uint32_t(c.routeSmelterIdx));
    putU32(b, uint32_t(c.routeMine));
    putU32(b, uint32_t(c.routeSmelter));
}

// Devolve o nº de bytes consumidos, ou 0 se o pacote está truncado.
size_t getCommand(const uint8_t* p, size_t n, CommandPacket& c) {
    size_t o = 0;
    auto need = [&](size_t k) { return o + k <= n; };
    if (!need(8)) return 0;
    c.type = int(getU32(p + o)); o += 4;
    const uint32_t nu = getU32(p + o); o += 4;
    if (nu > 4096 || !need(nu * 4)) return 0;
    c.unitIds.clear();
    for (uint32_t i = 0; i < nu; ++i) { c.unitIds.push_back(int(getU32(p + o))); o += 4; }
    if (!need(4)) return 0;
    const uint32_t nw = getU32(p + o); o += 4;
    if (nw > 4096 || !need(nw * 4)) return 0;
    c.waypoints.clear();
    for (uint32_t i = 0; i < nw; ++i) { c.waypoints.push_back(getU32(p + o)); o += 4; }
    if (!need(20)) return 0;
    c.patrolStart     = int(getU32(p + o)); o += 4;
    c.routeMineIdx    = int(getU32(p + o)); o += 4;
    c.routeSmelterIdx = int(getU32(p + o)); o += 4;
    c.routeMine       = int(getU32(p + o)); o += 4;
    c.routeSmelter    = int(getU32(p + o)); o += 4;
    return o;
}

// Cabeçalho do pacote de turno, com os campos do slot do original:
// magia, tipo 0x33, jogador, TICK DE DESTINO, nº de comandos.
std::vector<uint8_t> buildTurn(int player, int tick, const std::vector<CommandPacket>& cmds) {
    std::vector<uint8_t> b;
    putU32(b, kMagic);
    putU32(b, uint32_t(kPktTurn));
    putU32(b, uint32_t(player));
    putU32(b, uint32_t(tick));
    putU32(b, uint32_t(cmds.size()));
    for (const CommandPacket& c : cmds) putCommand(b, c);
    return b;
}

void sendRaw(const std::vector<uint8_t>& b) {
    if (s_sock == INVALID_SOCKET) return;
    sendto(s_sock, reinterpret_cast<const char*>(b.data()), int(b.size()), 0,
           reinterpret_cast<const sockaddr*>(&s_peer), sizeof(s_peer));
}

// Lê tudo que chegou. Pacotes repetidos são inofensivos: o turno é identificado
// por (tick, jogador) e reescrever com o mesmo conteúdo não muda nada.
void pump() {
    if (s_sock == INVALID_SOCKET) return;
    uint8_t buf[2048];
    for (;;) {
        sockaddr_in from{};
        int flen = sizeof(from);
        const int n = recvfrom(s_sock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                               reinterpret_cast<sockaddr*>(&from), &flen);
        if (n <= 0) return;
        if (n < 20 || getU32(buf) != kMagic) continue;
        const uint32_t type = getU32(buf + 4);
        if (type != uint32_t(kPktTurn)) continue;
        const int player = int(getU32(buf + 8));
        const int tick = int(getU32(buf + 12));
        const uint32_t count = getU32(buf + 16);
        if (player < 0 || player >= s_players || count > 256) continue;
        std::vector<CommandPacket> cmds;
        size_t o = 20;
        bool ok = true;
        for (uint32_t i = 0; i < count; ++i) {
            CommandPacket c;
            const size_t used = getCommand(buf + o, size_t(n) - o, c);
            if (!used) { ok = false; break; }
            o += used;
            cmds.push_back(std::move(c));
        }
        if (ok) s_turns[{tick, player}] = std::move(cmds);
    }
}

int turnTickFor(int tick) { return ((tick / kTurnPeriod) + kTurnLead) * kTurnPeriod; }

} // namespace

bool netStart(const NetConfig& cfg, std::string& error) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { error = "WSAStartup falhou"; return false; }
    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock == INVALID_SOCKET) { error = "socket falhou"; return false; }

    sockaddr_in me{};
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = INADDR_ANY;
    me.sin_port = htons(u_short(cfg.host ? cfg.port : cfg.port + 1));
    if (bind(s_sock, reinterpret_cast<sockaddr*>(&me), sizeof(me)) != 0) {
        error = "bind falhou"; closesocket(s_sock); s_sock = INVALID_SOCKET; return false;
    }

    s_peer.sin_family = AF_INET;
    s_peer.sin_port = htons(u_short(cfg.host ? cfg.port + 1 : cfg.port));
    const char* h = cfg.peerHost.empty() ? "127.0.0.1" : cfg.peerHost.c_str();
    if (inet_pton(AF_INET, h, &s_peer.sin_addr) != 1) {
        error = "endereco invalido"; closesocket(s_sock); s_sock = INVALID_SOCKET; return false;
    }

    // Aperto de mão: os dois mandam HELLO até ouvirem o outro.
    u_long nb = 1;
    ioctlsocket(s_sock, FIONBIO, &nb);
    std::vector<uint8_t> hello;
    putU32(hello, kMagic); putU32(hello, uint32_t(kPktHello));
    putU32(hello, uint32_t(cfg.localPlayer)); putU32(hello, 0); putU32(hello, 0);
    bool heard = false;
    for (int tries = 0; tries < 600 && !heard; ++tries) {
        sendRaw(hello);
        for (int k = 0; k < 20 && !heard; ++k) {
            uint8_t buf[256];
            sockaddr_in from{}; int flen = sizeof(from);
            const int n = recvfrom(s_sock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                                   reinterpret_cast<sockaddr*>(&from), &flen);
            if (n >= 20 && getU32(buf) == kMagic && getU32(buf + 4) == uint32_t(kPktHello))
                heard = true;
            else Sleep(5);
        }
    }
    if (!heard) {
        error = "o outro lado nao respondeu"; closesocket(s_sock); s_sock = INVALID_SOCKET; return false;
    }
    // Responde mais algumas vezes, para o outro lado também ouvir.
    for (int k = 0; k < 10; ++k) sendRaw(hello);

    s_localPlayer = cfg.localPlayer;
    s_players = 2;
    s_active = true;
    s_pending.clear(); s_turns.clear(); s_sentTurns.clear();
    s_lastAnnounced = -1;
    s_waits = 0;
    std::cout << "[Net] conectado; jogador local " << s_localPlayer << std::endl;
    return true;
}

void netStop() {
    if (s_sock != INVALID_SOCKET) { closesocket(s_sock); s_sock = INVALID_SOCKET; }
    if (s_active) WSACleanup();
    s_active = false;
    s_pending.clear(); s_turns.clear(); s_sentTurns.clear();
}

bool netActive() { return s_active; }
int  netLocalPlayer() { return s_localPlayer; }
int  netPlayerCount() { return s_players; }
int  netWaitCount() { return s_waits; }

void commandSubmit(int owner, const CommandPacket& pkt) {
    if (!s_active || owner != s_localPlayer) { processCommandPacket(owner, pkt); return; }
    s_pending.push_back(pkt);     // sai no próximo anúncio de turno
}

bool netReadyForTick(int tick) {
    if (!s_active) return true;
    pump();

    // Anuncia o nosso turno assim que ele vira (inclusive vazio: o turno vazio
    // é o que diz ao outro lado que pode seguir).
    const int mine = turnTickFor(tick);
    if (mine != s_lastAnnounced) {
        // Anuncia TODOS os turnos ate o atual, nao so o ultimo: os primeiros
        // (0, periodo, ...) saem vazios, e sem eles o outro lado trava logo no
        // comeco esperando um turno que ninguem mandou.
        const int from = s_lastAnnounced < 0 ? 0 : s_lastAnnounced + kTurnPeriod;
        for (int t = from; t <= mine; t += kTurnPeriod) {
            std::vector<CommandPacket> cmds;
            if (t == mine) { cmds.swap(s_pending); }
            const std::vector<uint8_t> b = buildTurn(s_localPlayer, t, cmds);
            s_turns[{t, s_localPlayer}] = std::move(cmds);
            s_sentTurns[t] = b;
            sendRaw(b);
        }
        s_lastAnnounced = mine;
    }

    // Só os ticks que começam um turno precisam esperar.
    if (tick % kTurnPeriod != 0) return true;
    bool ready = true;
    for (int p = 0; p < s_players; ++p)
        if (!s_turns.count({tick, p})) ready = false;
    if (!ready) {
        ++s_waits;
        // UDP perde pacote: reenvia o que já anunciamos para destravar.
        for (const auto& kv : s_sentTurns) sendRaw(kv.second);
    }
    return ready;
}

void netApplyTurn(int tick) {
    if (!s_active || tick % kTurnPeriod != 0) return;
    // Ordem fixa por número de jogador — as duas máquinas aplicam igual.
    for (int p = 0; p < s_players; ++p) {
        auto it = s_turns.find({tick, p});
        if (it == s_turns.end()) continue;
        for (const CommandPacket& c : it->second) processCommandPacket(p, c);
    }
    // Limpa o que já passou (deixa a folga do anel, como os 16 slots).
    const int keep = tick - kTurnPeriod * kTurnRing;
    for (auto it = s_turns.begin(); it != s_turns.end();)
        it = it->first.first < keep ? s_turns.erase(it) : std::next(it);
    for (auto it = s_sentTurns.begin(); it != s_sentTurns.end();)
        it = it->first < keep ? s_sentTurns.erase(it) : std::next(it);
}

} // namespace op2
