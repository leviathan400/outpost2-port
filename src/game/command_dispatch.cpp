#include "command.h"

#include "command_apply.h"
#include "construction.h"
#include "docking.h"
#include "earthwork.h"
#include "mining.h"
#include "missile.h"
#include "production.h"
#include "repair.h"
#include "research_lab.h"
#include "scenario.h"
#include "unit.h"
#include "weapon.h"
#include "colony.h"
#include "cheat.h"
#include "player.h"
#include "tethys_game.h"
#include "../net/net_game.h"

namespace op2 {

// Porta o resto de sub_40E210: o switch pelo tipo do pacote. O caso de
// movimento fica em command.cpp (loc_40E2C0); tudo o mais cai aqui, e cada
// caso so chama o modulo que ja sabia fazer aquilo.
//
// sub_40E210: o ANEL ANTI-DUPLICATA de comandos ([_Player+0xBD8], 16 turnos).
//
// Dez comandos passam por ele — os `case` de sub_40E210 que tocam
// `[ebp+eax*4+0BD8h]`: 0x09 produzir, 0x0A..0x0D as cargas, 0x12 e 0x13 ociosar
// e religar, 0x16 pesquisar, 0x17 treinar e 0x19 lançar. Antes de aplicar, o
// original vê se a MESMA unidade já foi assunto de um desses comandos na janela
// recente; se já foi, o comando é DESCARTADO — `loc_40E93C` devolve 1 e o
// despachante sai sem aplicar nada. É a proteção contra comando repetido: o
// clique duplo, ou o pacote que chegou duas vezes.
//
// Vale SÓ para jogador humano (`[_Player+0x28]`, o `test edx,edx` logo antes):
// o computador não passa pelo anel.
//
// A janela foi conferida instrução por instrução, porque não é a óbvia. Com
// `turno = tique >> log2(período)`, ele confere os slots de `turno−4` a
// `turno−2` — três, porque `dword_56DED8` é o mesmo kTurnLead — e GRAVA no slot
// do `turno`. O turno corrente e o anterior ficam de fora da conferência, e
// isso casa com o lockstep: o comando aplicado no turno T foi emitido em
// T−kTurnLead, logo a janela cobre o turno da emissão e os dois anteriores.
//
// Uma diferença proposital: o índice do original sai de um `%` COM SINAL, então
// nos primeiros turnos ele lê alguns dwords ANTES do anel — campos vizinhos do
// jogador. Aqui o índice é normalizado para 0..15. Na prática dá no mesmo (os
// slots começam zerados e nada casa), sem ler fora do vetor.
static bool usaAnel(int tipo) {
    switch (tipo) {
        case CMD_PRODUCE: case CMD_XFER_CARGO: case CMD_LOAD_CARGO:
        case CMD_UNLOAD_CARGO: case 0x0D: case CMD_IDLE: case CMD_UNIDLE:
        case CMD_RESEARCH: case CMD_TRAIN: case CMD_LAUNCH:
            return true;
        default:
            return false;
    }
}

static bool comandoDuplicado(int owner, const CommandPacket& p) {
    const int player = owner & 0x0F;
    if (player < 0 || player >= kMaxPlayers) return false;
    if (!g_playerData[player].isHuman) return false;
    if (p.unitIds.empty()) return false;
    // A chave é `[unidade+0x10]`, que no original é o próprio índice da unidade
    // no vetor — e é isso que o porte usa nos pacotes. O slot vazio vale 0, e
    // o índice 0 não é unidade válida, então não há falso positivo.
    const int chave = p.unitIds[0];
    int* anel = g_playerData[player].cmdRing;
    auto m16 = [](int v) { return ((v % 16) + 16) % 16; };
    const int turno = TethysGame::tick() / kTurnPeriod;
    const int fim = m16(turno - 1);
    int i = m16(fim - kTurnLead - 1);
    if (i != fim) {                                  // cmp eax, ecx / jz
        do {
            if (anel[i] == chave) return true;       // jz loc_40E93C
            i = m16(i + 1);
        } while (i != fim);
    }
    anel[m16(turno)] = chave;
    return false;
}

// A ordem dos casos segue o enum CommandType do original.
void dispatchCommandPacket(int owner, const CommandPacket& p) {
    if (usaAnel(p.type) && comandoDuplicado(owner, p)) return;
    switch (p.type) {
        case CMD_DOZE:
            if (!p.unitIds.empty()) applyDoze(owner, p.unitIds[0], p.rectL, p.rectT, p.rectR, p.rectB);
            return;
        case CMD_STOP:          applyStop(owner, p.unitIds); return;
        case CMD_BUILD:
            for (int id : p.unitIds)
                processBuildPacket(owner, id, p.waypoints.empty() ? 0u : p.waypoints[0],
                                   p.rectL, p.rectT, p.rectR, p.rectB, p.arg0);
            return;
        case CMD_BUILD_WALL:
            for (int id : p.unitIds)
                if ((g_units[id].owner & 0x0F) == (owner & 0x0F))
                    earthworkWallOrder(id, p.rectL, p.rectT, p.rectR, p.rectB, p.arg0);
            return;
        case CMD_REMOVE_WALL:   applyRemoveWall(owner, p.unitIds, p.rectL, p.rectT); return;
        case CMD_PRODUCE:
            if (!p.unitIds.empty()) processProducePacket(owner, p.unitIds[0], p.arg0, p.arg1, p.arg2);
            return;
        case CMD_XFER_CARGO:
            if (!p.unitIds.empty()) processBayTransfer(owner, p.unitIds[0], p.arg0);
            return;
        case CMD_LOAD_CARGO:
            if (!p.unitIds.empty()) applyLoadCargo(owner, p.unitIds[0]);
            return;
        case CMD_UNLOAD_CARGO:
            if (!p.unitIds.empty()) applyUnloadCargo(owner, p.unitIds[0]);
            return;
        case CMD_DUMP_CARGO:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    dumpCargoOrder(id);
            return;
        case CMD_IDLE:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    structureDoIdle(id);
            return;
        case CMD_UNIDLE:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    structureDoActivate(id);
            return;
        case CMD_SELF_DESTRUCT: applySelfDestruct(owner, p.unitIds); return;
        case CMD_SCATTER:       applyScatter(owner, p.unitIds); return;
        case CMD_RESEARCH:
            if (!p.unitIds.empty()) processStartResearch(owner, p.unitIds[0], p.arg0, p.arg1);
            return;
        case CMD_TRAIN:
            if (!p.unitIds.empty()) processTrainPacket(owner, p.unitIds[0], p.arg0);
            return;
        case CMD_LAUNCH:
            if (!p.unitIds.empty()) spaceportLaunch(p.unitIds[0], p.arg0, p.arg1);
            return;
        case CMD_REPAIR_OBJ:
        case CMD_REPROGRAM:
        case CMD_DISMANTLE:
            processRepairPacket(owner, p.type, p.unitIds, p.waypoints, p.target);
            return;
        case CMD_SALVAGE:
            applySalvage(owner, p.unitIds, p.rectL, p.rectT, p.rectR, p.rectB);
            return;
        case CMD_LIGHT_TOGGLE:
            for (int id : p.unitIds)
                if (id > 0 && id < static_cast<int>(g_units.size()) && (g_units[id].owner & 0x0F) == (owner & 0x0F))
                    unitSetLights(id, p.arg0);
            return;
        case CMD_ATTACK:
        case CMD_GUARD:
            processAttackPacket(owner, p.type, p.unitIds, p.waypoints, p.target);
            return;
        case CMD_TRANSFER: applyTransfer(owner, p.unitIds, p.arg0); return;
        case CMD_CHAT: {
            // loc_40FC96: sub_58642A roda ANTES do teste da máscara de
            // destinatários, então o cheat vale em TODAS as máquinas (o
            // lockstep continua de pé) e o texto exibido virou "Woof Woof!".
            std::string linha = p.text;
            cheatCheckChat(owner, linha);
            applyChat(owner, p.arg0, linha);
            return;
        }
        case CMD_ALLY:  applyAlly(owner, p.arg0); return;
        case CMD_QUIT:  applyQuit(owner, p.arg0, p.arg1); return;
        default: return;                       // Nop e os que o porte nao usa
    }
}

} // namespace op2
