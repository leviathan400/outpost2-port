#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "location.h"

namespace op2 {

// Sistema de comando do engine (porta a cadeia CommandPacket → sub_40E210).
// No original todo comando (jogador/IA/rede) é serializado num CommandPacket
// (header 0x0E + dados) e despachado por sub_40E210 sobre o objeto de comando
// do jogador (dword_56E2DC + owner*777*4). Aqui o packet é uma struct com os
// MESMOS campos (o formato de bits do LOCATION é idêntico ao do asm). A
// serialização em bytes (sub_490840/490880/410210) é detalhe de transporte e
// não precisa ser byte a byte a do original: está em src/net/net_game.cpp, no
// formato deste porte, e é o que leva TODO comando pelo turno em rede.

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

// Tipos de comando ([packet+0]). A tabela inteira do original (o enum
// CommandType: Nop = 0 ... InvalidCommand = 0x35); aqui estao os que o porte
// despacha. MOVE = 2, e sub_40E210 agrupa 2-4 e 0x26/0x27/0x28 no mesmo
// caminho de ordem de movimento.
enum CommandType : int {
    CMD_NOP          = 0x00,
    CMD_DOZE         = 0x01,
    CMD_MOVE         = 0x02,
    CMD_DOCK         = 0x03,
    CMD_DOCK_EG      = 0x04,
    CMD_STOP         = 0x05,
    CMD_BUILD        = 0x06,
    CMD_BUILD_WALL   = 0x07,
    CMD_REMOVE_WALL  = 0x08,
    CMD_PRODUCE      = 0x09,
    CMD_XFER_CARGO   = 0x0A,
    CMD_LOAD_CARGO   = 0x0B,
    CMD_UNLOAD_CARGO = 0x0C,
    CMD_DUMP_CARGO   = 0x0E,
    CMD_IDLE         = 0x12,
    CMD_UNIDLE       = 0x13,
    CMD_SELF_DESTRUCT= 0x14,
    CMD_SCATTER      = 0x15,
    CMD_RESEARCH     = 0x16,
    CMD_TRAIN        = 0x17,
    CMD_TRANSFER     = 0x18,
    CMD_LAUNCH       = 0x19,
    CMD_REPAIR_OBJ   = 0x1C,
    CMD_REPROGRAM    = 0x1D,
    CMD_DISMANTLE    = 0x1E,
    CMD_SALVAGE      = 0x1F,
    CMD_LIGHT_TOGGLE = 0x23,
    CMD_ATTACK       = 0x24,
    CMD_GUARD        = 0x25,
    CMD_STAND_GROUND = 0x26,
    CMD_CARGO_ROUTE  = 0x27,
    CMD_PATROL       = 0x28,
    CMD_CHAT         = 0x30,
    CMD_QUIT         = 0x31,
    CMD_ALLY         = 0x32,
};

// QuitCommand+0: como o jogador sai da partida (o menu END MISSION).
enum QuitMethod : int { QUIT_SELF_DESTRUCT = 0, QUIT_TRANSFER = 1, QUIT_GO_AI = 2 };

// CommandPacket — header 0x0E + dados. Campos equivalentes ao layout do asm:
//   [+0]=tipo, [+4]=tamanho; dados: [count][ids...][waypoints...].
struct CommandPacket {
    int type = 0;                      // [packet+0]
    std::vector<int> unitIds;          // lista de unidades-alvo do comando
    std::vector<uint32_t> waypoints;   // LOCATIONs empacotados (destino/rota)
    int patrolStart = 0;               // 0x28: 1ª palavra após os pontos → unidade+0x5E
    // 0x27 (rota de minério): 4 palavras após os pontos → +0x62/+0x64/+0x66/+0x68.
    int routeMineIdx = 0, routeSmelterIdx = 0, routeMine = 0, routeSmelter = 0;
    // --- corpo dos demais pacotes -------------------------------------------
    // O alvo do CommandTarget {u16 tileX|unitID, u16 tileY} — unidade com a
    // parte alta em 0xFFFF, tile com x | y << 16. (0x1C/0x1D/0x1E/0x24/0x25.)
    uint32_t target = 0;
    // PackedMapRect (0x01 dozer, 0x06 construir, 0x07 muro, 0x08 tirar muro,
    // 0x1F recuperar).
    int rectL = 0, rectT = 0, rectR = 0, rectB = 0;
    // Campos curtos do corpo, por tipo:
    //   0x06 arg0 = grupo da IA           0x07 arg0 = tipo de muro/tubo
    //   0x09 arg0 = item, arg1 = arma, arg2 = grupo
    //   0x0A arg0 = baia                  0x16 arg0 = assunto, arg1 = cientistas
    //   0x17 arg0 = alunos                0x18 arg0 = p/ qual jogador
    //   0x19 arg0/arg1 = px/py do alvo    0x1F arg0 = GORF
    //   0x23 arg0 = luzes ligadas         0x30 arg0 = máscara de destinatários
    //   0x31 arg0 = QuitMethod, arg1 = atraso   0x32 arg0 = com quem
    int arg0 = 0, arg1 = 0, arg2 = 0;
    std::string text;                  // ChatCommand: a linha digitada
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
// (porta sub_40E210). O caso MOVE (loc_40E2C0) mora aqui; os demais tipos vão
// para o despachante de command_dispatch.cpp, que chama o módulo de cada um.
struct UnitRecord;
// sub_43A910: larga a ordem e o alvo da unidade.
void resetUnitOrder(UnitRecord& u);

void processCommandPacket(int owner, const CommandPacket& pkt);
// Os casos que não são de movimento (sub_40E210 fora de loc_40E2C0).
void dispatchCommandPacket(int owner, const CommandPacket& pkt);

} // namespace op2
