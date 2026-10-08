#pragma once

#include <cstdint>
#include <vector>

namespace op2 {

struct UnitRecord;

// Criação de unidades e terreno pelos scripts de missão — porta das rotinas do
// EXE que os exports TethysGame/Unit usam. Coordenadas em tiles do PORT (a
// ponte em src/mission/op2_api.cpp converte o x interno do original).

// Registro da tabela de unidades da missão (UnitBlock, 32 bytes).
struct MissionUnitRecord {
    int     type;          // +0x00
    int     x, y;          // +0x04 / +0x08 (coordenadas internas do original)
    int     unk0C;         // +0x0C
    int     rotation;      // +0x10 (byte)
    int     weapon;        // +0x14 arma / carga
    int     unitClass;     // +0x18 (sub_49D1B0)
    int16_t cargoType;     // +0x1C
    int16_t cargoAmount;   // +0x1E
};
static_assert(sizeof(MissionUnitRecord) == 32, "UnitRecord da missao: 32 bytes");

// sub_4366D0: espiral a partir de (x,y) até uma célula livre e transitável p/
// a classe de movimento do tipo.
void findVehicleSpot(int type, int& x, int& y);
// sub_436820: espiral até caber um retângulo w2×h2 livre (classe do ConVec).
void findBuildingSpot(int& x, int& y, int w2, int h2);
// sub_4369E0: cria o prédio centrado em (x,y): terraplana o anel em volta,
// marca o footprint (cellType 0x1A + bloqueio, menos a doca), conectores de
// tubo (Border) e, nas minas, o veio do beacon embaixo. 0 se falhar.
int placeBuilding(int type, int x, int y, int owner);
// TethysGame::CreateUnit (@478 7E0). Devolve o id (0 se falhou).
int scenarioCreateUnit(int type, int x, int y, int owner, int cargo, int rotation);
// sub_49D280: unidade de um registro do UnitBlock (x,y já convertidos).
int createUnitFromRecord(const MissionUnitRecord& rec, int x, int y, int owner, int lightsOn);
// TethysGame::CreateBeacon. Devolve 1 se criou.
int createBeacon(int type, int x, int y, int commonRare, int yield, int variant);
// Beacon visível (pesquisado) p/ o jogador (vt[40] = sub_4054E0).
bool beaconSurveyed(int id, int player);

// Unit::DoSetLights / SetTruckCargo / SetFactoryCargo.
void unitSetLights(int id, int on);
void unitSetTruckCargo(int id, int cargoType, int amount);
void unitSetFactoryCargo(int id, int bay, int type, int cargo);

// _Player::CenterViewOn do jogador local (sub_408120): centro da vista em
// pixels do port; a GameState consome o pedido.
void requestCenterView(int px, int py);
bool takeCenterViewRequest(int& px, int& py);

// UnitBlock::SortAndInit com o qsort do CRT original (_qsort/_shortsort/_swap).
void unitBlockSort(MissionUnitRecord* table, int& numUnits, int ranges[16][2]);

// [+0x14]/[+0x18] do original (centro; prédio: ((tile*2 + (tam & 1)) << 4)),
// com x nas coordenadas do port.
void unitOrigPixel(const UnitRecord& r, int& x, int& y);

// sub_437700 + sub_437770: unidades vivas (flags 0x46, sem 0x20000, sub-estado
// != 0x0D) do dono em playerMask e com flags & flagMask, na ordem da lista por
// Y, com y em [y, y+h) e x em [x, x+w) — px INTERNOS (x dá a volta).
void mapUnitsInRect(int x, int y, int w, int h, uint32_t playerMask, uint32_t flagMask, std::vector<int>& out);

} // namespace op2
