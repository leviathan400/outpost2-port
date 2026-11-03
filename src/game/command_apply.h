#pragma once

#include <string>
#include <vector>

namespace op2 {

// Aplicacao dos CommandPackets que nao sao de movimento (sub_40E210 fora de
// loc_40E2C0). Cada uma roda DEPOIS que o pacote chegou pelo turno, nunca no
// clique: e isso que mantem as maquinas de uma partida em rede no mesmo passo.
// As que ja tinham nome proprio ficaram onde estavam (processProducePacket,
// processRepairPacket, processAttackPacket, structureDoIdle, ...); aqui estao
// as que eram o corpo de uma funcao "send*".

// 0x01 Doze (sub_423780): so a primeira unidade, com a regiao em ladrilhos.
void applyDoze(int owner, int id, int l, int t, int r, int b);
// 0x08 RemoveWall (sub_423500): Earthworker, o ladrilho em build*.
void applyRemoveWall(int owner, const std::vector<int>& ids, int x, int y);
// 0x05 Stop (sub_455540 -> case 5).
void applyStop(int owner, const std::vector<int>& ids);
// 0x14 SelfDestruct (sub_4555F0 -> case 20).
void applySelfDestruct(int owner, const std::vector<int>& ids);
// 0x15 Scatter (sub_4241D0): espalha a partir do centro do grupo.
void applyScatter(int owner, const std::vector<int>& ids);
// 0x1F Salvage (sub_4237B0): um caminhao, a regiao e o GORF.
void applySalvage(int owner, const std::vector<int>& ids, int l, int t, int r, int b);
// 0x0B / 0x0C: carregar e descarregar o caminhao atracado.
void applyLoadCargo(int owner, int buildingId);
void applyUnloadCargo(int owner, int buildingId);

// 0x18 Transfer (case 24): as unidades vivas da lista mudam de dono, com o
// aviso dos dois lados ("Transfer completed to" / "Transfer received from").
void applyTransfer(int owner, const std::vector<int>& ids, int to);

// 0x30 Chat (case 48): "<nome>: <linha>" no registro, se eu estiver na mascara.
void applyChat(int from, int dstMask, const std::string& line);
// 0x32 Ally (case 50): allyMask[alvo] ^= 1 << remetente, com as mensagens dos
// dois lados e as unidades do remetente largando o que miravam no alvo.
void applyAlly(int from, int to);
// 0x31 Quit (case 49): como o jogador sai -- destruindo tudo, doando ou
// virando IA.
void applyQuit(int owner, int method, int delay);

// --- o lado do CLIQUE desses tres ------------------------------------------
// A mascara dos "chat partners" (dword_56DEEC): com quem a minha linha fala.
// E local -- cada um escolhe a sua -- e viaja dentro do pacote de conversa.
int  chatMask();
void chatMaskToggle(int player);
// sub_4115F0 (a linha de conversa do jogo): manda o pacote 0x30.
void sendChat(const std::string& line);
// sub_457D20 com a parte alta 1: manda o pacote 0x32 para alternar a alianca.
void sendAlly(int to);
// sub_455950: doa as unidades escolhidas para outro jogador (pacote 0x18).
void sendTransfer(const std::vector<int>& ids, int to);
// A tela END MISSION: manda o 0x31 com o metodo escolhido.
void sendQuit(int method);

} // namespace op2
