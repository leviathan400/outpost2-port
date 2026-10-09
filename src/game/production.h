#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Lista de produção das fábricas — porta o painel de produção do EXE
// (dword_5674E8, vtable off_4D5808) e o comando "pode produzir"
// (dword_564A40, vtable off_4D4FF8). Item = tipo | (arma << 16).
//
// Fábricas: 0x2D Arachnid, 0x2E Consumer, 0x2F Structure, 0x30 Vehicle,
// 0x37 Spaceport (switch de sub_468120).

// sub_468120 (vt13): itens disponíveis da fábrica, na ordem da tabela fixa.
std::vector<uint32_t> factoryProduceList(int factoryId);
// sub_4567B0 (dword_564A40 vt0): a fábrica pode produzir o tipo?
bool factoryCanProduce(int factoryId, int type);
// sub_468430 (vt14): o jogador tem o minério (e, p/ itens especiais, o resto)?
bool produceAffordable(int factoryId, uint32_t item);
// sub_4685C0 (provedor vt5): nome do item na lista ("Lynx/Laser").
std::string produceItemName(uint32_t item);
// sub_4686E0 (provedor vt6): "Nome: 600 Common, 125 Rare".
std::string produceItemCost(uint32_t item, int player);

// BUILD IT (sub_456B50): pacote 9 {fábrica, tipo, arma, grupo 0xFFFF} do
// jogador local, processado na hora (sub_40E210 case 9).
void sendProduce(int factoryId, uint32_t item);
// sub_40E210 case 9: fábrica parada e operando, minério do dono suficiente →
// ação 9 e desconta o custo. false se recusou.
bool processProducePacket(int owner, int factoryId, int type, int weapon, int group);
// sub_43ACA0 case 9 com [+0x20] (início): sub-estado 7 com a duração.
void factoryProduceStart(int factoryId);
// sub_43DE50 case 7: conta a duração só com a fábrica operando.
void factoryProduceCountdown(int factoryId);
// sub_43ACA0 jpt_43B312 case 9 (fim): entrega o produto.
void factoryProduceFinish(int factoryId);

} // namespace op2
