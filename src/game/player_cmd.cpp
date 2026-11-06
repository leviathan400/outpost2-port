#include "command_apply.h"

#include <string>

#include "../audio/sound.h"
#include "command.h"
#include "../net/net_game.h"
#include "messages.h"
#include "player.h"
#include "repair.h"
#include "structure.h"
#include "tethys_game.h"
#include "trigger.h"
#include "unit.h"

namespace op2 {

// Os tres comandos que nao falam de uma unidade e sim do JOGADOR: a linha de
// conversa (0x30), a alianca (0x32) e a saida da partida (0x31). Todos vem de
// sub_40E210 (casos 48, 50 e 49).

// A mascara de destinatarios comeca com TODOS marcados: o original a mantem em
// dword_56DEEC e as caixas da aba Communications so a alternam.
static int s_chatMask = 0x7F;

int  chatMask() { return s_chatMask; }
void chatMaskToggle(int player) {
    if (player >= 0 && player < kMaxPlayers) s_chatMask ^= 1 << player;
}

void sendChat(const std::string& line) {                    // sub_4115F0
    if (line.empty()) return;
    CommandPacket pkt;
    pkt.type = CMD_CHAT;
    pkt.arg0 = s_chatMask;
    pkt.text = line.substr(0, 0x3F);                        // o original corta em 0x3F
    commandSubmit(TethysGame::localPlayer(), pkt);
}

void sendAlly(int to) {                                     // sub_457D20 (parte alta 1)
    CommandPacket pkt;
    pkt.type = CMD_ALLY;
    pkt.arg0 = to;
    commandSubmit(TethysGame::localPlayer(), pkt);
}

void sendTransfer(const std::vector<int>& ids, int to) {    // sub_455950
    if (ids.empty()) return;
    CommandPacket pkt;
    pkt.type = CMD_TRANSFER;
    pkt.unitIds = ids;
    pkt.arg0 = to;
    commandSubmit(TethysGame::localPlayer(), pkt);
}

void sendQuit(int method) {                                 // sub_457880 casos 9/0xB/0xC
    CommandPacket pkt;
    pkt.type = CMD_QUIT;
    pkt.arg0 = method;
    // sub_48A5D0 recebe o atraso junto do metodo, e ele NAO e o mesmo nos
    // tres: a autodestruicao usa 4 (caso 0xB) e os outros dois, 2.
    pkt.arg1 = method == QUIT_SELF_DESTRUCT ? 4 : 2;
    commandSubmit(TethysGame::localPlayer(), pkt);
}

// sub_4909F0: varre os jogadores e devolve true se algum OUTRO humano tem o
// meu bit na mascara de aliancas. Sem aliado nao ha para quem transferir, e o
// botao "&TRANSFER" nasce desabilitado.
bool quitTransferAvailable() {
    const int me = TethysGame::localPlayer();
    for (int i = 0; i < TethysGame::noPlayers() && i < kMaxPlayers; ++i) {
        if (i == me || !g_playerData[i].isHuman) continue;
        if (g_playerData[i].allyMask & (1u << me)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 0x30 Chat (case 48). O pacote leva o remetente, a mascara de destinatarios
// (os "chat partners" que ele marcou) e ate 0x3F caracteres. Quem nao esta na
// mascara simplesmente ignora. O texto entra no registro com x = -2, y = -1,
// que e a marca que a aba Communications usa para NAO pintar a linha de
// vermelho, e com o som 0x5D.
void applyChat(int from, int dstMask, const std::string& line) {
    const int me = TethysGame::localPlayer();
    if (!((dstMask >> me) & 1)) return;
    const std::string t = playerName(from) + ": " + line;
    postMessage(-2, -1, t.c_str(), 0x5D);
}

// ---------------------------------------------------------------------------
// 0x32 Ally (case 50). O remetente ALTERNA o proprio bit na mascara do outro:
// allyMask[to] ^= 1 << from. Como a mascara de X diz quem nao atira em X, e
// isso que faz as unidades do remetente pararem de atacar o alvo.
//
// Depois vem o aviso: quem RECEBEU a alianca ouve "<nome> has allied with you"
// (voz 0xCA+remetente) ou " has dissolved an alliance with you" (0xD0+...), e
// quem a DECLAROU le "Alliance formed/broken with <nome>" (voz 0xB2/0xB8 +
// alvo). Por fim, as unidades do remetente que estavam mirando alguem do alvo
// largam o alvo -- senao a alianca so valeria para as ordens novas.
void applyAlly(int from, int to) {
    if (from < 0 || from >= kMaxPlayers || to < 0 || to >= kMaxPlayers) return;
    const uint32_t bit = 1u << from;
    const bool tinha = (g_playerData[to].allyMask & bit) != 0;
    g_playerData[to].allyMask ^= bit;

    const int me = TethysGame::localPlayer();
    if (to == me) {
        const std::string t = playerName(from) +
                              (tinha ? " has dissolved an alliance with you" : " has allied with you");
        postMessage(-1, -1, t.c_str(), (tinha ? 0xD0 : 0xCA) + from);
    } else if (from == me) {
        const std::string t = std::string(tinha ? "Alliance broken with " : "Alliance formed with ") +
                              playerName(to);
        postMessage(-1, -1, t.c_str(), (tinha ? 0xB8 : 0xB2) + to);
    }

    // As unidades do remetente largam o que miravam no alvo.
    for (size_t i = 1; i < g_units.size(); ++i) {
        UnitRecord& u = g_units[i];
        if (u.id == -1 || (u.flags & UNIT_DEAD) || (u.flags & 0x20000u)) continue;
        if ((u.owner & 0x0F) != from) continue;
        // Alvo de combate [+0x4C]: solta a referencia e esquece.
        if (u.hasTarget() && u.targetIsUnit()) {
            const int t = static_cast<uint16_t>(u.target4C);
            if (t > 0 && t < static_cast<int>(g_units.size()) && (g_units[t].owner & 0x0F) == to) {
                if (g_units[t].refCount) --g_units[t].refCount;
                u.field4E = 0x7FFF;
            }
        }
        // Alvo da ORDEM [+0x48]: so quando a ordem era justamente atacar.
        if ((u.field4A & 0x8000) && u.action == CMD_ATTACK) {
            const int t = static_cast<uint16_t>(u.orderLo);
            if (t > 0 && t < static_cast<int>(g_units.size()) && (g_units[t].owner & 0x0F) == to) {
                u.action = CMD_STOP;
                u.field20 = 1;
                u.field3C = 0;
                resetUnitOrder(u);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 0x18 Transfer (case 24). Cada unidade viva da lista -- e sem a flag 0x20000,
// que marca quem esta saindo do mapa -- troca de dono por vt[0x68]. Os avisos
// vao para os dois lados, com a voz 0xBE+destino para quem doou e 0xC4+origem
// para quem recebeu.
void applyTransfer(int owner, const std::vector<int>& ids, int to) {
    if (to < 0 || to >= kMaxPlayers) return;
    for (int id : ids) {
        if (id <= 0 || id >= static_cast<int>(g_units.size())) continue;
        UnitRecord& u = g_units[id];
        if (u.id == -1 || (u.flags & 0x20000u)) continue;
        vehicleTransfer(id, to);
    }
    const int me = TethysGame::localPlayer();
    if ((owner & 0x0F) == me) {
        const std::string t = "Transfer completed to " + playerName(to);
        postMessage(-1, -1, t.c_str(), 0xBE + to);
    } else if (to == me) {
        const std::string t = "Transfer received from " + playerName(owner & 0x0F);
        postMessage(-1, -1, t.c_str(), 0xC4 + (owner & 0x0F));
    }
}

// ---------------------------------------------------------------------------
// 0x31 Quit (case 49). O corpo tem so o METODO de saida: 0 destroi tudo que e
// seu, 1 doa as unidades e 2 entrega a colonia a IA. Os tres sao itens da tela
// END MISSION (sub_457880 casos 4, 7 e 8).
void applyQuit(int owner, int method, int delay) {
    const int me = TethysGame::localPlayer();
    if (method == QUIT_SELF_DESTRUCT) {
        for (size_t i = 1; i < g_units.size(); ++i) {
            UnitRecord& r = g_units[i];
            if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
            if ((r.owner & 0x0F) != (owner & 0x0F)) continue;
            if (!(r.flags & (UNIT_VEHICLE | UNIT_BUILDING))) continue;
            killUnit(static_cast<int>(i), 0);
        }
    } else if (method == QUIT_TRANSFER) {
        // Doa o que e dele para o primeiro aliado vivo; sem aliado, some com
        // tudo (e o que sobra de sub_457880 caso 7 quando ninguem aceita).
        int destino = -1;
        for (int j = 0; j < kMaxPlayers && destino < 0; ++j)
            if (j != (owner & 0x0F) && g_playerData[j].allyMask & (1u << (owner & 0x0F))) destino = j;
        for (size_t i = 1; i < g_units.size(); ++i) {
            UnitRecord& r = g_units[i];
            if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
            if ((r.owner & 0x0F) != (owner & 0x0F)) continue;
            if (!(r.flags & (UNIT_VEHICLE | UNIT_BUILDING))) continue;
            if (destino < 0) killUnit(static_cast<int>(i), 0);
            else vehicleTransfer(static_cast<int>(i), destino);
        }
    } else {
        // GoAI: a colonia fica, so deixa de ser humana.
        g_playerData[owner & 0x0F].isHuman = 0;
    }
    const std::string t = playerName(owner) + " has left the game";
    postMessage(-1, -1, t.c_str(), 0);
    // loc_40FD54: quem saiu encerra -- em QUALQUER metodo, o GoAI do "BACK TO
    // MAIN MENU" inclusive (o original nao testa o metodo aqui) -- e em
    // multijogador com o codigo 9 em vez do 6.
    if ((owner & 0x0F) == me) endGamePlayerQuit(delay);
}

} // namespace op2
