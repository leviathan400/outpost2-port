#pragma once
#include <cstdint>
#include <string>

namespace op2 {

// Os cheats digitados na linha de conversa (sub_58642A, no segmento DSEG do
// OUTPOST2.EXE). O jogador escreve a frase no chat, aperta Enter, e o pacote
// 0x30 passa pelo reconhecedor ANTES do teste da máscara de destinatários —
// logo o cheat vale em TODAS as máquinas e o lockstep da rede continua de pé.
// Quem recebe o efeito é o REMETENTE (`[pacote+0x0E]`, que o original preenche
// com dword_56DEE8). Casando, o texto é substituído por "Woof Woof!", que é o
// que todos leem no chat.
//
// As frases NÃO existem no binário: só os quatro hashes de 32 bits ficam
// guardados (VA 0x5863B8), e os handlers são registrados em bloco por
// sub_58650C. Por isso `grep` no executável não acha nada.
//
// O original não consulta `CanAllowCheats` aqui — ela é só exportada para as
// DLLs de missão. Não há porteiro: no jogo de 1997 o cheat funciona em
// qualquer partida, inclusive em rede.
//
// ESTE PORTE NÃO: como o OPUPatch, os cheats valem só em JOGO SOLO. Numa
// partida de rede ou num cenário multijogador a frase não faz nada e segue
// como mensagem comum de chat. Ver `cheatsAllowed`.

// Os cheats estão liberados nesta partida? Falso em rede (`netActive`) e em
// cenário multijogador (`multiplayerMission`, o sub_4027D0 do original). A
// resposta é a MESMA em todas as máquinas, então o lockstep não corre risco.
bool cheatsAllowed();

// sub_58642A: o hash de uma frase. Bytes depois do primeiro zero não entram.
uint32_t cheatHash(const char* text);

// O reconhecedor inteiro. Se `linha` for um cheat, aplica o efeito em `from`,
// troca `linha` por "Woof Woof!" e devolve true.
bool cheatCheckChat(int from, std::string& linha);

} // namespace op2
