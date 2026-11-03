#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

struct AIModDesc;

// GameStartInfo / MissionResults — no original é a MESMA estrutura de 0x3DD
// bytes: o shell a monta (New Campaign), entrega em StartSingleGame (sub_4893A0
// a copia para TethysGame+0xA4), o jogo a preenche no fim (sub_489A30) e a
// devolve ao shell em ActivateShell(motivo, resultados) → CreateShell, que a
// guarda em 0x13015a34 para a próxima missão.
struct GameStartInfo {
    uint32_t flags = 0;            // +0x00: 8 = campanha, 0x10 = multijogador
    // +0x0C bit 3: a partida e o DEMO DE ATRACAO. O shell a liga ao disparar
    // ademo%d.dll; copiada para dword_56DF08, ela muda o fim da missao
    // (sub_403110: vitoria -> codigo 11 em vez de 3/7).
    bool demo = false;
    std::string dll;               // +0x10 nome da DLL da missão
    uint32_t playerWord[6] = {};   // +0x30: bit 2 Eden, bits 3-4 dificuldade, 5-7 cor
    int endCode = 0;               // +0xBA (TethysGame+0x9C)
    // +0xC2: SETE dwords por jogador (6 jogadores, 28 bytes cada), copiados de
    // [jogador+0x1BC] no fim de sub_489A30. E daqui que a tela "MULTIPLAYER
    // GAME STATISTICS" do shell (Dialog 155) tira os seis numeros; o setimo e
    // a soma minerio comum + raro, que o original escreve por cima.
    enum { kStatCount = 7 };
    int stats[6][kStatCount] = {};
    int missionNumber = 0;         // +0x16A campanha: missões concluídas; senão o tipo
    int food = 0, ore = 0, rareOre = 0;            // +0x16E/+0x172/+0x176
    int workers = 0, scientists = 0, kids = 0;     // +0x17A/+0x17E/+0x182
    uint32_t spaceWord = 0;        // +0x186 = [jogador+0x08] (RLV, satélites, módulos)
    int localPlayer = 0;           // +0x18A
    int endTick = 0;               // +0x18B
    int playerBlock[0x12] = {};    // +0x18F = [jogador+0x34..+0x7B] (moral)
    int techCount = 0;             // +0x1D7 nº de técnicas da árvore − 1
    int16_t techLevel = 0;         // +0x1DB
    std::vector<uint8_t> techIdx;  // +0x1DD técnicas pesquisadas (ordem topológica)
    std::vector<uint8_t> techMask; // +0x2DD jogadores que as pesquisaram

    bool campaign() const { return (flags & 8) != 0; }
    bool eden() const { return (playerWord[0] & 4) != 0; }
    int  difficulty(int p) const { return static_cast<int>((playerWord[p] & 0x18) >> 3); }
};

// sub_4898F0: na campanha, as sobras da missão anterior vão para o jogador
// local antes do InitProc (recursos, população, +0x08 com os RLVs em voo, o
// bloco de moral) e a pesquisa volta (sub_473770).
void startInfoApplyCampaign(const GameStartInfo& gsi);

// sub_489A30: monta os resultados no fim da partida. Na campanha, sem sucesso
// (código != 3), só o código e o tick mudam; senão entram as sobras do jogador
// local — RLV num Spaceport conta como RLV, e a carga dele (EDWARD, satélite,
// módulos 0x5A..0x63) como lançada — e a pesquisa (sub_473640).
void startInfoCollectResults(GameStartInfo& gsi, const AIModDesc* desc, int endCode);

} // namespace op2
