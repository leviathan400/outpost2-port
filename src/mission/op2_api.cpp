// Exports do OUTPOST2.EXE que as DLLs de missão importam ("Outpost2.exe!<nome
// decorado>"), reimplementados com a MESMA convenção de chamada do original
// (o port é x86/MSVC como o jogo):
//   QAE (métodos __thiscall)  → __fastcall(this, edx ignorado, args...)
//   SI/YI (static/global __fastcall) → __fastcall com os mesmos parâmetros;
//   retorno de Trigger por valor → ponteiro oculto em ECX (o primeiro param).
// O comportamento segue as rotinas do EXE (citadas em cada função); layouts de
// classe conferidos no SDK Outpost2DLL só como referência.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "mission.h"
#include "../game/colony.h"
#include "../game/game_map.h"
#include "../game/messages.h"
#include "../game/player.h"
#include "../game/scenario.h"
#include "../game/tethys_game.h"
#include "../game/trigger.h"
#include "../game/unit.h"
#include "../game/walls.h"
#include "../game/disaster.h"

namespace op2 {

namespace {

// _Player exportado (?Player@@3PAV_Player@@A): 12 bytes por jogador
// (playerNum + checkValue[8]); a DLL usa Player + 0xC*i como `this`.
struct DllPlayer { int playerNum; signed char checkValue[8]; };
static_assert(sizeof(DllPlayer) == 12, "_Player: 12 bytes");
DllPlayer g_dllPlayer[7];

struct ApiLoc { int x, y; };   // LOCATION por valor (8 bytes, na pilha)

// UnitBlock (0x88 bytes): nº de unidades, faixas por classe, tabela.
struct DllUnitBlock { int numUnits; int ranges[16][2]; MissionUnitRecord* table; };
static_assert(sizeof(DllUnitBlock) == 0x88, "UnitBlock: 0x88 bytes");

// x interno do original → x do port (mapas estreitos guardados a partir de
// dword_54EBF0; demais dão a volta pela máscara).
int portX(int x) {
    const int off = g_gameMap.internalXOffset();
    return (off > 0 ? x - off : x) & (g_gameMap.width() - 1);
}
// x de CreateUnit: preso a [x1, larguraInterna/2 + x1] nos mapas estreitos.
int portXClamped(int x) {
    const int off = g_gameMap.internalXOffset();
    if (off <= 0) return x & (g_gameMap.width() - 1);
    if (x < off) x = off;
    if (x > g_gameMap.width() + off) x = g_gameMap.width() + off;
    return (x - off) & (g_gameMap.width() - 1);
}

// ---- _Player ------------------------------------------------------------------
void __fastcall P_GoEden(DllPlayer* p, void*)                 { Player[p->playerNum].GoEden(); }
void __fastcall P_GoPlymouth(DllPlayer* p, void*)             { Player[p->playerNum].GoPlymouth(); }
void __fastcall P_SetColorNumber(DllPlayer* p, void*, int v)  { Player[p->playerNum].SetColorNumber(v); }
void __fastcall P_SetOre(DllPlayer* p, void*, int v)          { Player[p->playerNum].SetOre(v); }
int  __fastcall P_Difficulty(DllPlayer* p, void*)             { return Player[p->playerNum].Difficulty(); }   // +0x0C
void __fastcall P_SetRareOre(DllPlayer* p, void*, int v)      { Player[p->playerNum].SetRareOre(v); }
void __fastcall P_SetFoodStored(DllPlayer* p, void*, int v)   { Player[p->playerNum].SetFoodStored(v); }
void __fastcall P_SetKids(DllPlayer* p, void*, int v)         { Player[p->playerNum].SetKids(v); }
void __fastcall P_SetWorkers(DllPlayer* p, void*, int v)      { Player[p->playerNum].SetWorkers(v); }
void __fastcall P_SetScientists(DllPlayer* p, void*, int v)   { Player[p->playerNum].SetScientists(v); }
void __fastcall P_SetSolarSat(DllPlayer* p, void*, int v)     { Player[p->playerNum].SetSolarSat(v); }
void __fastcall P_SetTechLevel(DllPlayer* p, void*, int v)    { Player[p->playerNum].SetTechLevel(v); }
void __fastcall P_MarkResearchComplete(DllPlayer* p, void*, int t) { Player[p->playerNum].MarkResearchComplete(t); }
// ?CenterViewOn: só o jogador local; centro em (x<<5, y<<5) (sub_408120).
void __fastcall P_CenterViewOn(DllPlayer* p, void*, int x, int y) {
    if (p->playerNum != TethysGame::localPlayer()) return;
    requestCenterView(portX(x) << 5, y << 5);
}

// ---- TethysGame -----------------------------------------------------------------
int __fastcall T_CreateUnit(int* unit, int type, ApiLoc loc, int owner, int cargo, int rotation) {
    const int id = scenarioCreateUnit(type, portXClamped(loc.x), loc.y, owner, cargo, rotation);
    if (!id) return 0;
    *unit = id;
    return 1;
}
int __fastcall T_CreateBeacon(int type, int x, int y, int commonRare, int yield, int variant) {
    return createBeacon(type, portX(x), y, commonRare, yield, variant);
}
int __fastcall T_CreateWallOrTube(int x, int y, int /*unused*/, int type) {
    return createWallOrTube(portX(x), y, type);
}
// sub_49D540: resolve o UnitBlock exportado e cria do último ao primeiro.
int __fastcall T_CreateUnitBlock(DllPlayer* player, const char* exportName, int lightsOn) {
    MissionScript* m = currentMission();
    auto* blk = m ? static_cast<DllUnitBlock*>(m->proc(exportName ? exportName : "")) : nullptr;
    if (!blk) {
        std::cerr << "[Mission] Couldn't resolve data reference " << (exportName ? exportName : "")
                  << " in mission script" << std::endl;
        return 0;
    }
    int n = 0;
    for (int i = blk->numUnits - 1; i >= 0; --i) {
        const MissionUnitRecord& r = blk->table[i];
        if (createUnitFromRecord(r, portX(r.x), r.y, player->playerNum, lightsOn)) ++n;
    }
    return n;
}
void __fastcall T_ForceMoraleGreat(int p)           { TethysGame::ForceMoraleGreat(p); }
void __fastcall T_ForceMoraleGood(int p)            { TethysGame::ForceMoraleGood(p); }
void __fastcall T_ForceMoraleOK(int p)              { TethysGame::ForceMoraleOK(p); }
void __fastcall T_ForceMoralePoor(int p)            { TethysGame::ForceMoralePoor(p); }
void __fastcall T_ForceMoraleRotten(int p)          { TethysGame::ForceMoraleRotten(p); }
// ?AddMessage(x, y, msg, jogador, som): só p/ todos (−1) ou o jogador local;
// x/y em pixels (x interno) → sub_438F70.
void __fastcall T_AddMessage(int x, int y, char* msg, int toPlayer, int sound) {
    if (toPlayer != -1 && toPlayer != TethysGame::localPlayer()) return;
    postMessage(x - g_gameMap.internalXOffset() * 32, y, msg, sound);
}
int  __fastcall T_GetRand(int n)                    { return TethysGame::getRand(n); }
void __fastcall T_SetCheatUnlimitedResources(int v) { TethysGame::setCheatFlag(0, v); }
void __fastcall T_SetCheatProduceAll(int v)         { TethysGame::setCheatFlag(1, v); }
void __fastcall T_SetCheatFastUnits(int v)          { TethysGame::setCheatFlag(4, v); }
void __fastcall T_SetCheatFastProduction(int v)     { TethysGame::setCheatFlag(5, v); }
void __fastcall T_SetDaylightEverywhere(int v)      { TethysGame::SetDaylightEverywhere(v); }
void __fastcall T_SetDaylightMoves(int v)           { TethysGame::SetDaylightMoves(v); }
// ?SetEarthquake: unidade-desastre 0x4E (sub_4466B0(0x4E, x<<5, y<<5, 6, mag)).
void __fastcall T_SetEarthquake(int x, int y, int magnitude) {
    setEarthquake(portX(x), y, magnitude);
}
// ?SetMeteor / ?SetEruption: 0x50 / 0x4F (cargo = tamanho / velocidade da lava).
void __fastcall T_SetMeteor(int x, int y, int size)    { setMeteor(portX(x), y, size); }
void __fastcall T_SetEruption(int x, int y, int speed) { setEruption(portX(x), y, speed); }
// ?SetLightning: 0x4C (cargo = duração); destino em +0x64/+0x66 se endX != -1.
void __fastcall T_SetLightning(int x, int y, int duration, int endX, int endY) {
    setLightning(portX(x), y, duration, endX == -1 ? -1 : portX(endX), endY);
}
// ?SetTornado: 0x4D (cargo = duração); destino em +0x65/+0x67; imediato pula os avisos.
void __fastcall T_SetTornado(int x, int y, int duration, int endX, int endY, int immediate) {
    setTornado(portX(x), y, duration, endX == -1 ? -1 : portX(endX), endY, immediate);
}
// ?SetLavaSpeed (sub_432DE0 → sub_49E240).
void __fastcall T_SetLavaSpeed(int speed) { setLavaSpeed(speed); }
// ?SetMusicPlayList (sub_451090): sem áudio no port.
void __fastcall T_SetMusicPlayList(int, int, int*) {}

// ---- GameMap ------------------------------------------------------------------------
// ?InitialSetTile (sub_4353A0): só o tileIndex (bits 5..15).
void __fastcall M_InitialSetTile(ApiLoc loc, int tile) {
    const int x = portX(loc.x);
    const uint32_t c = g_gameMap.rawCell(x, loc.y);
    g_gameMap.setRawCell(x, loc.y, (c & 0xFFFF001Fu) | ((static_cast<uint32_t>(tile) << 5) & 0xFFE0u));
}
// ?SetTile (sub_435330 + redesenho da vista).
void __fastcall M_SetTile(ApiLoc loc, int tile) { M_InitialSetTile(loc, tile); }
// ?SetLavaPossible: bit 0x10000000 da célula; nos mapas estreitos só dentro de
// [x1, x1 + larguraInterna/2).
void __fastcall M_SetLavaPossible(ApiLoc loc, int on) {
    const int off = g_gameMap.internalXOffset();
    if (off > loc.x) return;
    if (off && off + g_gameMap.internalWidth() / 2 <= loc.x) return;
    setLavaPossible(portX(loc.x), loc.y, on != 0);
}
// ?SetCellType: troca os 5 bits baixos.
void __fastcall M_SetCellType(ApiLoc loc, int type) {
    const int x = portX(loc.x);
    const uint32_t c = g_gameMap.rawCell(x, loc.y);
    g_gameMap.setRawCell(x, loc.y, (c & ~0x1Fu) | (static_cast<uint32_t>(type) & 0x1Fu));
}
// ?SetInitialLightLevel: negativo soma a largura (interna); fase = pos<<16 / W.
void __fastcall M_SetInitialLightLevel(int pos) {
    const int W = g_gameMap.internalWidth();
    if (pos < 0) pos += W;
    TethysGame::setDaylightPhase(W ? (pos << 16) / W : 0, W);
}

// ---- Unit ------------------------------------------------------------------------------
void __fastcall U_DoSetLights(int* u, void*, int on)                    { unitSetLights(*u, on); }
void __fastcall U_SetFactoryCargo(int* u, void*, int bay, int t, int c) { unitSetFactoryCargo(*u, bay, t, c); }
void __fastcall U_SetTruckCargo(int* u, void*, int t, int amount)       { unitSetTruckCargo(*u, t, amount); }
// ?SetCargo: [+0x24] = carga; ConVec (2) guarda a arma do kit em +0x4F.
void __fastcall U_SetCargo(int* u, void*, int cargo, int weapon) {
    UnitRecord& r = g_units[*u];
    r.cargo = static_cast<uint16_t>(cargo);
    if (r.type == mapConVec) r.kitWeapon = static_cast<uint8_t>(weapon);
}
// ?PutInGarage(baia, x, y): vt[65] da garagem em (x,y) — garagem ainda não portada.
void __fastcall U_PutInGarage(int* u, void*, int bay, int x, int y) {
    std::cerr << "[Mission] PutInGarage(unidade " << *u << ", baia " << bay << ", " << x << "," << y
              << "): garagem nao portada" << std::endl;
}

// ---- UnitBlock / ScStub -----------------------------------------------------------------
DllUnitBlock* __fastcall UB_Ctor(DllUnitBlock* b, void*, MissionUnitRecord* table) {
    b->table = table;
    unitBlockSort(table, b->numUnits, b->ranges);
    return b;
}
void __fastcall SS_Dtor(int* stub, void*) { *stub = 0xFF; }   // ??1ScStub: [this] = 0xFF
int* __fastcall SS_Ctor(int* stub, void*) { *stub = 0xFF; return stub; }   // ??0ScStub
int  __fastcall TRG_HasFired(int* stub, void*, int player) { return triggerHasFired(*stub, player) ? 1 : 0; }

// ---- Triggers (retorno do Trigger por valor: ponteiro em ECX) -------------------------------
int* __fastcall TR_CreateTimeTrigger(int* ret, int bEnabled, int bOneShot, int time, const char* fn) {
    *ret = createTimeTrigger(bEnabled, bOneShot, time, fn);
    return ret;
}
int* __fastcall TR_CreateResearchTrigger(int* ret, int bEnabled, int bOneShot, int techID, int player, const char* fn) {
    *ret = createResearchTrigger(bEnabled, bOneShot, techID, player, fn);
    return ret;
}
int* __fastcall TR_CreateVictoryCondition(int* ret, int bEnabled, int bOneShot, int* trigger, const char* text) {
    *ret = createVictoryCondition(bEnabled, bOneShot, *trigger, text);
    return ret;
}
int* __fastcall TR_CreateFailureCondition(int* ret, int bEnabled, int bOneShot, int* trigger, const char* text) {
    *ret = createFailureCondition(bEnabled, bOneShot, *trigger, text);
    return ret;
}
int* __fastcall TR_CreateCountTrigger(int* ret, int bEnabled, int bOneShot, int player, int type, int cargo,
                                      int count, int compare, const char* fn) {
    *ret = createCountTrigger(bEnabled, bOneShot, player, type, cargo, count, compare, fn);
    return ret;
}
int* __fastcall TR_CreateKitTrigger(int* ret, int bEnabled, int bOneShot, int player, int type, int count, const char* fn) {
    *ret = createKitTrigger(bEnabled, bOneShot, player, type, count, fn);
    return ret;
}
int* __fastcall TR_CreateBuildingCountTrigger(int* ret, int bEnabled, int bOneShot, int player, int count,
                                              int compare, const char* fn) {
    *ret = createBuildingCountTrigger(bEnabled, bOneShot, player, count, compare, fn);
    return ret;
}
int* __fastcall TR_CreateVehicleCountTrigger(int* ret, int bEnabled, int bOneShot, int player, int count,
                                             int compare, const char* fn) {
    *ret = createVehicleCountTrigger(bEnabled, bOneShot, player, count, compare, fn);
    return ret;
}
int* __fastcall TR_CreateEscapeTrigger(int* ret, int bEnabled, int bOneShot, int player, int x, int y, int w, int h,
                                       int refValue, int type, int cargoType, int cargoAmount, const char* fn) {
    *ret = createEscapeTrigger(bEnabled, bOneShot, player, x, y, w, h, refValue, type, cargoType, cargoAmount, fn);
    return ret;
}
// ?CreateSetTrigger (__cdecl, variádico): retorno oculto no 1º arg; os triggers
// do conjunto vêm por valor (4 bytes = stubIndex).
int* __cdecl TR_CreateSetTrigger(int* ret, int bEnabled, int bOneShot, int total, int needed, const char* fn, ...) {
    std::vector<int> stubs(total > 0 ? total : 0);
    va_list ap;
    va_start(ap, fn);
    for (int i = 0; i < total; ++i) stubs[i] = va_arg(ap, int);
    va_end(ap);
    *ret = createSetTrigger(bEnabled, bOneShot, total, needed, fn, stubs.data());
    return ret;
}

const std::map<std::string, void*>& table() {
    static const std::map<std::string, void*> t = {
        {"?Player@@3PAV_Player@@A", &g_dllPlayer[0]},
        {"?GoEden@_Player@@QAEXXZ", reinterpret_cast<void*>(&P_GoEden)},
        {"?GoPlymouth@_Player@@QAEXXZ", reinterpret_cast<void*>(&P_GoPlymouth)},
        {"?SetColorNumber@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetColorNumber)},
        {"?SetOre@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetOre)},
        {"?Difficulty@_Player@@QBEHXZ", reinterpret_cast<void*>(&P_Difficulty)},
        {"?ForceMoraleGreat@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleGreat)},
        {"?ForceMoraleOK@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleOK)},
        {"?ForceMoralePoor@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoralePoor)},
        {"?ForceMoraleRotten@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleRotten)},
        {"?AddMessage@TethysGame@@SIXHHPADHH@Z", reinterpret_cast<void*>(&T_AddMessage)},
        {"?SetCargo@Unit@@QAEXW4map_id@@0@Z", reinterpret_cast<void*>(&U_SetCargo)},
        {"?PutInGarage@Unit@@QAEXHHH@Z", reinterpret_cast<void*>(&U_PutInGarage)},
        {"??0ScStub@@QAE@XZ", reinterpret_cast<void*>(&SS_Ctor)},
        {"?HasFired@Trigger@@QAEHH@Z", reinterpret_cast<void*>(&TRG_HasFired)},
        {"?CreateFailureCondition@@YI?AVTrigger@@HHAAV1@PBD@Z", reinterpret_cast<void*>(&TR_CreateFailureCondition)},
        {"?CreateCountTrigger@@YI?AVTrigger@@HHHW4map_id@@0HW4compare_mode@@PBD@Z", reinterpret_cast<void*>(&TR_CreateCountTrigger)},
        {"?CreateKitTrigger@@YI?AVTrigger@@HHHW4map_id@@HPBD@Z", reinterpret_cast<void*>(&TR_CreateKitTrigger)},
        {"?CreateBuildingCountTrigger@@YI?AVTrigger@@HHHHW4compare_mode@@PBD@Z", reinterpret_cast<void*>(&TR_CreateBuildingCountTrigger)},
        {"?CreateVehicleCountTrigger@@YI?AVTrigger@@HHHHW4compare_mode@@PBD@Z", reinterpret_cast<void*>(&TR_CreateVehicleCountTrigger)},
        {"?CreateEscapeTrigger@@YI?AVTrigger@@HHHHHHHHW4map_id@@HHPBD@Z", reinterpret_cast<void*>(&TR_CreateEscapeTrigger)},
        {"?CreateSetTrigger@@YA?AVTrigger@@HHHHPBDZZ", reinterpret_cast<void*>(&TR_CreateSetTrigger)},
        {"?SetRareOre@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetRareOre)},
        {"?SetFoodStored@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetFoodStored)},
        {"?SetKids@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetKids)},
        {"?SetWorkers@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetWorkers)},
        {"?SetScientists@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetScientists)},
        {"?SetSolarSat@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetSolarSat)},
        {"?SetTechLevel@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_SetTechLevel)},
        {"?MarkResearchComplete@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_MarkResearchComplete)},
        {"?CenterViewOn@_Player@@QAEXHH@Z", reinterpret_cast<void*>(&P_CenterViewOn)},
        {"?CreateUnit@TethysGame@@SIHAAVUnit@@W4map_id@@ULOCATION@@H1H@Z", reinterpret_cast<void*>(&T_CreateUnit)},
        {"?CreateBeacon@TethysGame@@SIHW4map_id@@HHHHH@Z", reinterpret_cast<void*>(&T_CreateBeacon)},
        {"?CreateWallOrTube@TethysGame@@SIHHHHW4map_id@@@Z", reinterpret_cast<void*>(&T_CreateWallOrTube)},
        {"?CreateUnitBlock@TethysGame@@SIHAAV_Player@@PBDH@Z", reinterpret_cast<void*>(&T_CreateUnitBlock)},
        {"?ForceMoraleGood@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleGood)},
        {"?GetRand@TethysGame@@SIHH@Z", reinterpret_cast<void*>(&T_GetRand)},
        {"?SetCheatFastProduction@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetCheatFastProduction)},
        {"?SetCheatFastUnits@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetCheatFastUnits)},
        {"?SetCheatProduceAll@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetCheatProduceAll)},
        {"?SetCheatUnlimitedResources@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetCheatUnlimitedResources)},
        {"?SetDaylightEverywhere@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetDaylightEverywhere)},
        {"?SetDaylightMoves@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetDaylightMoves)},
        {"?SetEarthquake@TethysGame@@SIXHHH@Z", reinterpret_cast<void*>(&T_SetEarthquake)},
        {"?SetMeteor@TethysGame@@SIXHHH@Z", reinterpret_cast<void*>(&T_SetMeteor)},
        {"?SetEruption@TethysGame@@SIXHHH@Z", reinterpret_cast<void*>(&T_SetEruption)},
        {"?SetLightning@TethysGame@@SIXHHHHH@Z", reinterpret_cast<void*>(&T_SetLightning)},
        {"?SetTornado@TethysGame@@SIXHHHHHH@Z", reinterpret_cast<void*>(&T_SetTornado)},
        {"?SetLavaSpeed@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetLavaSpeed)},
        {"?SetLavaPossible@GameMap@@SIXULOCATION@@H@Z", reinterpret_cast<void*>(&M_SetLavaPossible)},
        {"?SetMusicPlayList@TethysGame@@SIXHHPAW4SongIds@@@Z", reinterpret_cast<void*>(&T_SetMusicPlayList)},
        {"?InitialSetTile@GameMap@@SIXULOCATION@@H@Z", reinterpret_cast<void*>(&M_InitialSetTile)},
        {"?SetTile@GameMap@@SIXULOCATION@@H@Z", reinterpret_cast<void*>(&M_SetTile)},
        {"?SetCellType@GameMap@@SIXULOCATION@@H@Z", reinterpret_cast<void*>(&M_SetCellType)},
        {"?SetInitialLightLevel@GameMap@@SIXH@Z", reinterpret_cast<void*>(&M_SetInitialLightLevel)},
        {"?DoSetLights@Unit@@QAEXH@Z", reinterpret_cast<void*>(&U_DoSetLights)},
        {"?SetFactoryCargo@Unit@@QAEXHW4map_id@@0@Z", reinterpret_cast<void*>(&U_SetFactoryCargo)},
        {"?SetTruckCargo@Unit@@QAEXW4Truck_Cargo@@H@Z", reinterpret_cast<void*>(&U_SetTruckCargo)},
        {"??0UnitBlock@@QAE@PAUUnitRecord@@@Z", reinterpret_cast<void*>(&UB_Ctor)},
        {"??1ScStub@@QAE@XZ", reinterpret_cast<void*>(&SS_Dtor)},
        {"?CreateTimeTrigger@@YI?AVTrigger@@HHHPBD@Z", reinterpret_cast<void*>(&TR_CreateTimeTrigger)},
        {"?CreateResearchTrigger@@YI?AVTrigger@@HHHHPBD@Z", reinterpret_cast<void*>(&TR_CreateResearchTrigger)},
        {"?CreateVictoryCondition@@YI?AVTrigger@@HHAAV1@PBD@Z", reinterpret_cast<void*>(&TR_CreateVictoryCondition)},
    };
    return t;
}

} // namespace

void* op2ApiResolve(const std::string& name) {
    // Os objetos _Player exportados guardam o índice (sub_490190 / ctor estático).
    for (int i = 0; i < 7; ++i) {
        g_dllPlayer[i].playerNum = i;
        std::memset(g_dllPlayer[i].checkValue, -1, sizeof(g_dllPlayer[i].checkValue));
    }
    auto it = table().find(name);
    return it == table().end() ? nullptr : it->second;
}

} // namespace op2
