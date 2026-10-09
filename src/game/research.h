#pragma once

#include <string>
#include <vector>

namespace op2 {

// Árvore de pesquisa — porta do objeto dword_56B5F0 do OUTPOST2.EXE.
//   sub_4733B0  carrega o arquivo (TechtreeName da missão, no SHEETS.VOL) com
//               ID máximo = nívelMáx*1000+999 e troca o Research Topic dos
//               descritores (desc+0x1E4) pelo ÍNDICE da tecnologia;
//   sub_472880  lê todas as BEGIN_TECH (sub_4739C0), ordena por ID (_qsort),
//               põe a raiz virtual (índice 0, ID 0, pesquisada por todos) e
//               liga pré-requisitos (+0x38) e dependentes (+0x44);
//   sub_473860  marca a tecnologia p/ o jogador e aplica as melhorias;
//   sub_472D80  propaga: dependentes de custo 0 p/ a facção, dentro do nível
//               máximo e com todos os pré-requisitos ficam pesquisados;
//   sub_472C30  no início da partida, propaga da raiz p/ os jogadores 0..5;
//   sub_472F70  SetTechLevel; sub_473830 MarkResearchComplete;
//   sub_472C50  "tem a tecnologia" (com a exceção do Basic Lab).

struct TechUpgrade {
    int kind = 0;        // tipo do campo (tabela @4D5C40): 0 UNIT_PROP, 1 PLAYER_PROP, 2 FUNCTION_RESULT
    int offset = 0;      // offset no bloco 0x44 (UNIT_PROP) / registro do jogador (PLAYER_PROP)
    int unitType = -1;   // UNIT_PROP: map_id (sub_445010)
    int value = 0;
};

struct Tech {
    int id = 0;                  // +0x00
    int category = 0;            // +0x04
    int level = 0;               // +0x08 = id / 1000
    int plymouthCost = 0;        // +0x0C (COST preenche os dois)
    int edenCost = 0;            // +0x10
    int maxScientists = 0;       // +0x14
    int lab = 0;                 // +0x18
    unsigned researched = 0;     // +0x1C máscara de jogadores
    std::vector<int> requires;   // +0x24/+0x38 (índices)
    std::vector<TechUpgrade> upgrades;   // +0x20/+0x3C
    std::vector<int> dependents; // +0x40/+0x44 (índices)
    std::string name, description, teaser, improveDesc;   // +0x28.. +0x34
};

// sub_4733B0. false se o arquivo não abriu / erro de leitura.
bool loadTechTree(const std::string& fileName, int maxTechLevel);
void clearTechTree();
int  techCount();                         // [+0] (inclui a raiz)
const Tech* techAt(int index);
int  techIndex(int techID);               // sub_472CD0 (−1 se não existe)
int  researchTopicIndex(int type);        // desc+0x1E4 após o carregamento

void researchStartGame();                              // sub_472C30
void researchMarkComplete(int player, int index);      // sub_473830
void researchSetTechLevel(int player, int level);      // sub_472F70(player, level*1000)
bool researchHas(int player, int index);               // sub_472C50
// sub_473300: o jogador pode pesquisar o tópico? (não pesquisado, dentro do
// nível máximo, custo da facção != −1 e todos os pré-requisitos pesquisados)
bool researchAvailable(int player, int index);

} // namespace op2
