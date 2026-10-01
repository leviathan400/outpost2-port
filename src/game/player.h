#pragma once

#include <cstdint>
#include <set>

namespace op2 {

constexpr int kMaxPlayers = 7;   // OP2 suporta até 7 jogadores

// Status de comida (enum FoodStatus) e moral (enum MoraleLevels) do original —
// armazenados como int no registro do jogador (lidos por FoodSupply/MoraleLevel).
enum FoodStatus   { FOOD_RISING = 0, FOOD_STABLE = 1, FOOD_FALLING = 2 };
enum MoraleLevels { MORALE_ROTTEN = 0, MORALE_POOR = 1, MORALE_OK = 2,
                    MORALE_GOOD = 3, MORALE_GREAT = 4 };

// Dados por jogador. No original isto é um array global (base 0x56E2DC, stride
// 777 dwords por jogador); o _Player é só um wrapper do índice (this+0 = índice).
// Aqui mantemos o MESMO desenho (wrapper + array global), modelando os campos de
// economia/colônia decodificados do disassembly (offsets reais no comentário).
struct PlayerData {
    int difficulty  = 1;   // +0x0C (0=fácil,1=normal,2=difícil)
    int ore         = 0;   // +0x20 common ore
    int rareOre     = 0;   // +0x24 rare ore
    int foodStored  = 0;   // +0x10
    int foodSupply  = FOOD_STABLE;  // +0x4C
    int morale      = MORALE_OK;    // +0x34
    int isHuman     = 1;   // +0x28 (1=humano, 0=IA)
    int isEden      = 1;   // +0x2C (1=Eden, 0=Plymouth)
    int colorNumber = 0;   // +0x30
    int solarSat    = 0;   // +0x08 bits 4-7 (0..15)
    int rlvCount    = 0;   // +0x08 bits 0-3  (+0x00 é o objeto contador de RLV)
    int workers     = 0;   // +0x94
    int scientists  = 0;   // +0x98
    int kids        = 0;   // +0x9C
    int techLevel   = 0;   // gerido pelo subsistema de pesquisa (dword_56B5F0)
    std::set<int> research;  // tecnologias concluídas (MarkResearchComplete)
};

extern PlayerData g_playerData[kMaxPlayers];

// _Player — wrapper do índice do jogador (porta a classe _Player, asm @191940+;
// ver [[op2-simulation-core]]). Os métodos têm os nomes reais do original (as DLLs
// de missão chamam Player[i].SetOre(...), etc.).
class _Player {
public:
    _Player() : m_index(0) {}
    explicit _Player(int index) : m_index(index) {}   // ??0_Player(int)

    int playerIndex() const { return m_index; }
    PlayerData& d() const { return g_playerData[m_index]; }

    // --- economia (getters const / setters) ---
    int  Difficulty() const { return d().difficulty; }
    int  Ore() const        { return d().ore; }
    int  RareOre() const    { return d().rareOre; }
    int  FoodStored() const { return d().foodStored; }
    void SetOre(int v)        { d().ore = v; }
    void SetRareOre(int v)    { d().rareOre = v; }
    void SetFoodStored(int v) { d().foodStored = v; }
    void SetSolarSat(int v)   { d().solarSat = (v < 0 ? 0 : (v > 15 ? 15 : v)); }
    int  SolarSat() const     { return d().solarSat; }

    int  Workers() const    { return d().workers; }
    int  Scientists() const { return d().scientists; }
    int  Kids() const       { return d().kids; }
    void SetWorkers(int v)    { d().workers = v; }
    void SetScientists(int v) { d().scientists = v; }
    void SetKids(int v)       { d().kids = v; }

    int  MoraleLevel() const { return d().morale; }
    int  FoodSupply() const  { return d().foodSupply; }

    // --- colônia / controle ---
    int  IsHuman() const { return d().isHuman; }
    int  IsEden() const  { return d().isEden; }
    void GoAI()       { d().isHuman = 0; }
    void GoHuman()    { d().isHuman = 1; }
    void GoEden()     { d().isEden = 1; }   // escreve 1 em +0x2C
    void GoPlymouth() { d().isEden = 0; }   // escreve 0 em +0x2C
    void SetColorNumber(int v) { d().colorNumber = v; }
    int  ColorNumber() const { return d().colorNumber; }

    // --- pesquisa (subsistema central no original) ---
    void SetTechLevel(int v)        { d().techLevel = v; }
    int  TechLevel() const          { return d().techLevel; }
    void MarkResearchComplete(int t){ d().research.insert(t); }
    int  HasTechnology(int t) const { return d().research.count(t) ? 1 : 0; }

    int  GetRLVCount() const { return d().rlvCount; }

private:
    int m_index;   // this+0 = índice do jogador
};

// Array global de jogadores (a API de missão usa Player[i]).
extern _Player Player[kMaxPlayers];

// Zera e inicializa os jogadores no começo de uma partida.
void resetPlayers(int numPlayers);

} // namespace op2
