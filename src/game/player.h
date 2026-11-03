#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace op2 {

constexpr int kMaxPlayers = 7;   // OP2 suporta até 7 jogadores

// Status de comida (enum FoodStatus) e moral (enum MoraleLevels) do original —
// armazenados como int no registro do jogador (lidos por FoodSupply/MoraleLevel).
enum FoodStatus   { FOOD_RISING = 0, FOOD_STABLE = 1, FOOD_FALLING = 2 };
// MoraleLevel (+0x34) = índice da linha de MORALE.TXT (UTOPIA..WAYBAD), na
// ordem do enum do SDK: 0 = excelente ... 4 = péssimo.
enum MoraleLevels { MORALE_EXCELLENT = 0, MORALE_GOOD = 1, MORALE_FAIR = 2,
                    MORALE_POOR = 3, MORALE_TERRIBLE = 4 };

// Dados por jogador. No original isto é um array global (base 0x56E2DC, stride
// 777 dwords por jogador); o _Player é só um wrapper do índice (this+0 = índice).
// Aqui mantemos o MESMO desenho (wrapper + array global), modelando os campos de
// economia/colônia decodificados do disassembly (offsets reais no comentário).
struct PlayerData {
    uint32_t allyMask = 0; // +0x04 bitmask de aliança (dword_56E2E0; bit j = aliado de j),
                           // usado por sub_490C50 no pathfinder
    int difficulty  = 1;   // +0x0C (0=fácil,1=normal,2=difícil)
    int ore         = 0;   // +0x20 common ore
    int rareOre     = 0;   // +0x24 rare ore
    int foodStored  = 0;   // +0x10
    int foodSupply  = FOOD_STABLE;  // +0x4C
    int morale      = MORALE_FAIR;  // +0x34 (linha OKAYFINE)
    int isHuman     = 1;   // +0x28 (1=humano, 0=IA)
    int isEden      = 1;   // +0x2C (1=Eden, 0=Plymouth)
    int colorNumber = 0;   // +0x30
    // _Player::GetName (sub_490BA0): o nome que aparece no chat, na tela de
    // aliancas e nas mensagens de fim de partida. Em jogo solo fica vazio e
    // playerName() devolve "Player N".
    //
    // BUFFER FIXO de proposito: playersSerialize grava a struct inteira byte a
    // byte, entao um std::string aqui gravaria o ponteiro interno e corromperia
    // o heap ao carregar um nome maior que o SSO.
    char name[32] = {0};
    int solarSat    = 0;   // +0x08 bits 4-7 (0..15)
    int rlvCount    = 0;   // +0x08 bits 0-3  (+0x00 é o objeto contador de RLV)
    uint32_t spaceBits = 0;  // +0x08 bits 8..31: EDWARD (0x58) nos bits 9-12; demais
                             // tipos espaciais 1 bit cada (bit = tipo − 0x4D)
    int workers     = 0;   // +0x94
    int scientists  = 0;   // +0x98
    int kids        = 0;   // +0x9C
    int canAccumulateOre = 0;      // +0x188 (_Player::canAccumulateOre; liga c/ pesquisa)
    int canAccumulateRareOre = 0;  // +0x198 (_Player::canAccumulateRareOre)
    // Os seis contadores da tela "MULTIPLAYER GAME STATISTICS" (Dialog 155 do
    // OP2SHRES). Ao contrario do que esta sessao de 2026-10-29 concluiu, eles
    // NAO sao dados mortos: sub_489A30 copia os sete dwords de [jogador+0x1BC]
    // para MissionResults+0xC2 + jogador*28, e o shell le de la (o setimo e a
    // soma minerio comum + raro, posta por cima).
    int structuresBuilt = 0;    // +0x1BC
    int unitsBuilt = 0;         // +0x1C0
    int structuresLost = 0;     // +0x1C4
    int unitsLost = 0;          // +0x1C8
    int enemyStructuresDestroyed = 0;   // +0x1CC
    int enemyUnitsDestroyed = 0;        // +0x1D0
    int pathRefine  = 1;   // +0x1B8 refinamento de rota (sub_440660/sub_440C50);
                           // sub_490190 = 1, sub_471F30 recalcula
    int techLevel   = 0;   // gerido pelo subsistema de pesquisa (dword_56B5F0)
    std::set<int> research;  // tecnologias concluídas (MarkResearchComplete)
};

extern PlayerData g_playerData[kMaxPlayers];

// sub_490BA0: o nome do jogador, com o padrao do original quando nao ha um.
std::string playerName(int player);
void setPlayerName(int player, const std::string& name);

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
    void GoAI();      // sub_4905A0: humano → computador + grupo padrão (sub_402E70)
    void GoHuman();   // sub_4905E0: computador → humano, apaga o grupo padrão
    void GoEden()     { d().isEden = 1; }   // escreve 1 em +0x2C
    void GoPlymouth() { d().isEden = 0; }   // escreve 0 em +0x2C
    void SetColorNumber(int v) { d().colorNumber = v; }
    int  ColorNumber() const { return d().colorNumber; }

    // --- pesquisa (subsistema central no original) ---
    void SetTechLevel(int v);       // sub_472F70(jogador, v*1000)
    int  TechLevel() const          { return d().techLevel; }
    void MarkResearchComplete(int techID);   // sub_473830(jogador, índice)
    int  HasTechnology(int techID) const;    // sub_472C50(jogador, índice)

    int  GetRLVCount() const { return d().rlvCount; }

private:
    int m_index;   // this+0 = índice do jogador
};

// Array global de jogadores (a API de missão usa Player[i]).
extern _Player Player[kMaxPlayers];

// Zera e inicializa os jogadores no começo de uma partida.
void resetPlayers(int numPlayers);

// Unidades espaciais no +0x08 do registro: bits 0-3 RLV (0x6A), 4-7 SOLAR_SAT
// (0x59), 9-12 EDWARD (0x58); os outros tipos são 1 bit (tipo − 0x4D).
uint32_t playerSpaceWord(int player);

// _Player::hasVehicle(tipo, carga): veículo da lista (+0xC1C) do tipo com a
// carga (−1 = qualquer); conta também os guardados nas garagens 0x29, como
// loc_4775BD.
bool playerHasVehicle(int player, int type, int cargo);
// _Player::can* — cache[0..7] = bytes +4..+11 do _Player da DLL (0xFF = não
// calculado; resetChecks zera tudo p/ 0xFF): [0] canBuildBuilding, [1]
// canBuildVehicle, [3] canAccumulateOre, [5..7] canDoResearch (lab 1..3).
int playerCanBuildBuilding(int player, signed char* cache);
int playerCanBuildVehicle(int player, signed char* cache, int arg);
int playerCanAccumulateOre(int player, signed char* cache);
int playerCanAccumulateRareOre(int player, signed char* cache);
int playerCanBuildSpace(int player, signed char* cache);
int playerCanDoResearch(int player, signed char* cache, int techID);
int playerSpaceCount(int player, int type);   // sub_490800 (só 0x6A/0x59/0x58)
// sub_45D1B0: força de combate da unidade (tanques/Spider/Scorpion/Guard Post).
int unitStrength(int unitId);
// _Player::GetTotalPlayerStrength: soma de sub_45D1B0 nos veículos e prédios / 8.
int playerTotalStrength(int player);
int playerSpaceHas(int player, int type);     // sub_490780

} // namespace op2
