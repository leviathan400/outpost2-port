#pragma once

#include <vector>

#include "player.h"
#include "unit.h"

namespace op2 {

// Economia da colônia — porta sub_471F30 (chamada por jogador a cada tick no
// turno sub_489DD0, antes dos comandos e do laço de unidades sub_43AC30).
//
// FASE A (este módulo): lista de prédios por jogador, rede de tubos até o CC e
// alocação de ENERGIA / TRABALHADORES / CIENTISTAS (liga e desliga as flags
// 0x2000/0x4000/0x8000 dos prédios). FASE B (pendente): comida, população,
// moral, armazenamento e mensagens ("Power Levels: ...", "%i Structures
// Disabled") — trechos marcados no colony.cpp.

// Campos +0xA0.. do registro do jogador (stride 0xC24 a partir de 0x56E2DC).
struct ColonyState {
    int dirty = 1;               // +0xA0 dword_56E37C: recalcular a alocação
    int workersLeft = 0;         // +0xA4 trabalhadores sobrando
    int scientistsLeft = 0;      // +0xA8 cientistas sobrando
    int powerGenerated = 0;      // +0xAC energia gerada (geradores ativos + CC)
    int powerIdle = 0;           // +0xB0 capacidade dos geradores parados
    int powerRequired = 0;       // +0xB4 energia exigida pelos prédios
    int powerLeft = 0;           // +0xB8 energia sobrando
    int inactiveCount = 0;       // +0xBC prédios em construção/desmontagem/0x12
    int activeCount = 0;         // +0xC0 prédios operando
    int structCount = 0;         // +0xC4 prédios na lista
    int unpoweredCount = 0;      // +0xC8 prédios sem energia
    int workersRequired = 0;     // +0xCC
    int scientistsRequired = 0;  // +0xD0
    int scientistsAsWorkers = 0; // +0xD4 cientistas usados como trabalhadores
    int researchAssigned = 0;    // +0xD8 soma de vt+0xE4 (cientistas em pesquisa)
    int solarSatellites = 0;     // +0xEC unidades 0x59 do jogador (sub_490800)
};
extern ColonyState g_colony[kMaxPlayers];

// Lista de prédios do jogador ([jogador+0xC18], encadeada por [unit+0x0C]),
// ORDENADA por Resource Priority (desc+0x2A0): sub_439FE0 insere antes do 1º
// prédio de prioridade >= a do novo. sub_439C00 retira. Ambas marcam dirty.
void colonyStructureAdded(int id);
void colonyStructureRemoved(int id);
const std::vector<int>& colonyStructures(int player);
void colonyMarkDirty(int player);
void colonyReset();

// Tabela de moral (MORALE.TXT, sub_44E180): 5 linhas UTOPIA..WAYBAD, bônus em
// /256 = (pct*256+50)/100.
struct MoraleRow {
    int points = 0, powBonus = 0, rschBonus = 0, factBonus = 0, foodBonus = 0;
    int defectRate = 0, fertRate = 0, dieRate = 0;
};
bool loadMoraleSheet();
const MoraleRow& moraleRow(int level);

// vt+0xA0 por classe (sub_483750 base; sub_40C1B0 CC; sub_46E6D0 geradores;
// sub_46E880 Solar; sub_44AD60 minas/Magma Well).
bool structureOperational(const UnitRecord& r);

// Turno: sub_471F30 p/ cada jogador.
void tickColony();

} // namespace op2
