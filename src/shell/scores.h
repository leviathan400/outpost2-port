#pragma once

#include <string>
#include <vector>

namespace op2 {

// Placar dos jogos de colônia — seção [ColonyGameScores] do OUTPOST2.INI, no
// formato do OP2Shell.dll: `Count` e, por entrada i (1-based),
// `Game%i` (LevelDesc da missão), `Difficulty%i` ("Easy"/"Normal"/"Hard"),
// `Time%i` (marcas = tick/100, int), `Name%i` (até 20 caracteres) e
// `Date%i` ("%i-%i-%i" da data local).
struct ScoreEntry {
    std::string game;
    std::string difficulty;
    int         time = 0;      // marcas (tick/100)
    std::string name;
    std::string date;
};

// Lê as entradas na ordem gravada (1..Count).
std::vector<ScoreEntry> scoresLoad();
// Acrescenta uma entrada (Count+1) e grava o INI.
void scoresAdd(const ScoreEntry& e);
// "Delete All Scores?": zera Count.
void scoresClear();

// Linha da listbox do Dialog 223: os cinco campos unidos por ", ", com o tempo
// formatado em "%07i" (o original monta exatamente nesta ordem).
std::string scoreLine(const ScoreEntry& e);

// "Easy"/"Normal"/"Hard" pelos bits 3-4 da palavra do jogador (0 / 8 / outro).
const char* scoreDifficultyName(unsigned playerWord);
// Data local no formato "%i-%i-%i" (mês-dia-ano), como GetLocalTime do original.
std::string scoreToday();

} // namespace op2
