#pragma once

#include <string>

namespace op2 {

struct UnitRecord;

// EXTRA DESTE PORTE (chave "[Game] CommandDebug" do OUTPOST2.INI; o original
// nao tem nada disso). O original recusa em silencio boa parte dos comandos de
// predio: produzir, pesquisar, treinar. Com a chave ligada, cada recusa de um
// comando do jogador LOCAL vira uma linha no registro de mensagens (sem som,
// na posicao do predio, entao o clique na mensagem leva ate ele) e no stdout,
// com o motivo. So exibe: nao muda a simulacao, e o lockstep fica intacto.
void setCommandDebug(bool on);
bool commandDebug();

// Registra a recusa (se ligado e se o dono for o jogador local).
void commandRefused(int owner, int unitId, const char* why);

// Por que o predio nao esta operando (structureOperational falso): "being
// built", "no power", "not enough workers"... "" se esta operando.
const char* structureDownReason(const UnitRecord& r);

// A ultima recusa registrada (para os testes; "" se nenhuma desde o reset).
const std::string& commandLastRefusal();
void commandClearLastRefusal();

} // namespace op2
