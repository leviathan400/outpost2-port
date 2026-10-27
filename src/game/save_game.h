#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

class SaveStream;

// Jogo salvo — porta do fluxo de TApp::SaveGame / LoadGame: arquivos
// SGAME%d.OP2 (0..10), a assinatura "OUTPOST 2.00 SAVED GAME\x1A\0" (unk_4D6378)
// e as seções na ordem de sub_48A120. O conteúdo de cada seção é o estado do
// port (não o layout de memória do original).
constexpr int kSaveSlots = 11;
std::string saveSlotPath(int slot);                   // "SGAME%d.OP2"

struct SaveHeader {
    std::string description;   // texto do slot
    std::string dll;           // DLL da missão
    int64_t time = 0;          // _time() da gravação
    uint32_t tick = 0;
};
// Lê só o cabeçalho (lista de slots). false se o arquivo não é um jogo salvo.
bool saveReadHeader(const std::string& path, SaveHeader& out);

// Seções de cada subsistema (o mesmo código grava e lê).
void tethysSerialize(SaveStream& s);       // tethys_game.cpp (relógio, RNG, opções)
void mapSerialize(SaveStream& s);          // game_map.cpp (células vivas, luz)
void unitsSerialize(SaveStream& s);        // unit.cpp
void playersSerialize(SaveStream& s);      // player.cpp
void colonySerialize(SaveStream& s);       // colony.cpp
void researchSerialize(SaveStream& s);     // research.cpp
void sheetsSerialize(SaveStream& s);       // building_sheet.cpp (blocos por jogador)
void triggersSerialize(SaveStream& s);     // trigger.cpp (stubs: triggers, grupos, Pinwheel)
void messagesSerialize(SaveStream& s);     // messages.cpp
void disasterSerialize(SaveStream& s);     // disaster.cpp (lava, micróbio)
void dllPlayersSerialize(SaveStream& s);   // op2_api.cpp (_Player da DLL)

// Grava a partida em andamento (sub_48A120). `minimap` = estado do minimapa e
// do globo (sub_420580, logo depois da pesquisa); `extra` = o da interface.
bool saveGameWrite(const std::string& path, const std::string& description,
                   const std::vector<uint8_t>& minimap, const std::vector<uint8_t>& extra);
// Lê a partida depois que a DLL e o mapa da missão foram recarregados (sem
// InitProc). Devolve os dois blocos.
bool saveGameRead(const std::string& path, std::vector<uint8_t>& minimap,
                  std::vector<uint8_t>& extra);

} // namespace op2
