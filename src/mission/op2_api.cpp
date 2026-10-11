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
#include <new>
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
#include "../game/ai_group.h"
#include "../game/commands.h"
#include "../game/structure.h"
#include "../game/command.h"
#include "../game/missile.h"
#include "../audio/sound.h"

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
int  __fastcall P_Ore(DllPlayer* p, void*)                    { return g_playerData[p->playerNum].ore; }        // +0x20
int  __fastcall P_Kids(DllPlayer* p, void*)                   { return g_playerData[p->playerNum].kids; }       // +0x9C
int  __fastcall P_Workers(DllPlayer* p, void*)                { return g_playerData[p->playerNum].workers; }    // +0x94
int  __fastcall P_Scientists(DllPlayer* p, void*)             { return g_playerData[p->playerNum].scientists; } // +0x98
int  __fastcall P_HasTechnology(DllPlayer* p, void*, int id)  { return Player[p->playerNum].HasTechnology(id); }
// Cache dos can* nos bytes +4..+11 do _Player da DLL.
void __fastcall P_ResetChecks(DllPlayer* p, void*) { std::memset(p->checkValue, 0xFF, 8); }
int  __fastcall P_CanBuildBuilding(DllPlayer* p, void*) {
    return playerCanBuildBuilding(p->playerNum, p->checkValue);
}
int  __fastcall P_CanBuildVehicle(DllPlayer* p, void*, int arg) {
    return playerCanBuildVehicle(p->playerNum, p->checkValue, arg);
}
int  __fastcall P_CanAccumulateOre(DllPlayer* p, void*) {
    return playerCanAccumulateOre(p->playerNum, p->checkValue);
}
int  __fastcall P_CanAccumulateRareOre(DllPlayer* p, void*) {
    return playerCanAccumulateRareOre(p->playerNum, p->checkValue);
}
int  __fastcall P_CanBuildSpace(DllPlayer* p, void*) { return playerCanBuildSpace(p->playerNum, p->checkValue); }
int  __fastcall P_RareOre(DllPlayer* p, void*) { return g_playerData[p->playerNum].rareOre; }   // +0x24
int  __fastcall P_IsEden(DllPlayer* p, void*) { return g_playerData[p->playerNum].isEden; }     // +0x2C
// GetDefaultGroup: ScGroup por valor (retorno oculto na pilha) = stub do grupo padrão.
int* __fastcall P_GetDefaultGroup(DllPlayer* p, void*, int* ret) { *ret = aiDefaultGroup(p->playerNum); return ret; }
int  __fastcall P_CanDoResearch(DllPlayer* p, void*, int techID) {
    return playerCanDoResearch(p->playerNum, p->checkValue, techID);
}
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
// ?FreeMoraleLevel: comando 0x2B com valor 0x20 (sub_4229F0 case 32).
void __fastcall T_FreeMoraleLevel(int p)            { TethysGame::FreeMoraleLevel(p); }
void __fastcall T_SetMicrobeSpreadSpeed(int v)      { setMicrobeSpreadSpeed(v); }
// ?AddMessage(x, y, msg, jogador, som): só p/ todos (−1) ou o jogador local;
// x/y em pixels (x interno) → sub_438F70.
void __fastcall T_AddMessage(int x, int y, char* msg, int toPlayer, int sound) {
    if (toPlayer != -1 && toPlayer != TethysGame::localPlayer()) return;
    postMessage(x - g_gameMap.internalXOffset() * 32, y, msg, sound);
}
int  __fastcall T_GetRand(int n)                    { return TethysGame::getRand(n); }
int  __fastcall T_Tick()                            { return static_cast<int>(TethysGame::tick()); }
int  __fastcall T_NoPlayers()                       { return TethysGame::noPlayers(); }          // dword_56DEC0
// GameStartInfo (dword_56DEFC): bit0 desastres, bit1 dia/noite, bit2 moral fixa,
// bits 17..20 unidades iniciais.
int  __fastcall T_CanHaveDisasters()                { return TethysGame::startFlags() & 1; }
int  __fastcall T_UsesDayNight()                    { return (TethysGame::startFlags() & 2) >> 1; }
int  __fastcall T_UsesMorale()                      { return (TethysGame::startFlags() & 4) ? 0 : 1; }
int  __fastcall T_InitialUnits()                    { return (TethysGame::startFlags() & 0x1E0000) >> 17; }
// FindEMPMissleTarget(x1, y1, x2, y2, jogador): blocos 8x8 (tiles internos) no
// retângulo; pontua cada ocupante vivo (prédio só no seu tile central): armado
// (0x90) do jogador +0x40, armado de outro −0x20, desarmado de outro +1. Devolve
// o canto do melhor bloco + 3. (LOCATION por valor: ponteiro oculto em ECX.)
ApiLoc* __fastcall T_FindEMPMissleTarget(ApiLoc* ret, int x1, int y1, int x2, int y2, int player) {
    const int imask = g_gameMap.internalWidth() - 1;            // dword_54EBC4
    const int off = g_gameMap.internalXOffset();
    x1 &= ~7; y1 &= ~7;
    x2 = ((x2 + 7) & imask) & ~7;
    y2 = (y2 + 7) & ~7;
    ret->x = x1 + 3; ret->y = y1 + 3;
    if (x2 == x1 || y2 == y1) return ret;
    int best = -1, bx = x1, by = y1;
    int cx = x1;
    for (;;) {
        for (int cy = y1; cy < y2; cy += 8) {
            int score = 0;
            for (int x = cx; x != cx + 8; ++x)
                for (int y = cy; y != cy + 8; ++y) {
                    if (y < 0 || y >= g_gameMap.height()) continue;
                    const int id = g_gameMap.cellOccupant(portX(x & imask), y);
                    if (!id || id >= static_cast<int>(g_units.size())) continue;
                    const UnitRecord& r = g_units[id];
                    if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
                    if ((r.flags & UNIT_BUILDING) &&
                        (((unitOX(r) + off * 32) >> 5) != x || (unitOY(r) >> 5) != y)) continue;
                    const int ow = r.owner & 0x0F;
                    if (r.flags & 0x90u) score += ow == player ? 0x40 : -0x20;
                    else if (ow != player) ++score;
                }
            if (score > best) { best = score; bx = cx; by = cy; }
        }
        cx = (cx + 8) & imask;
        if (cx == x2) break;
    }
    ret->x = bx + 3; ret->y = by + 3;
    return ret;
}
int  __fastcall T_Time()                            { return static_cast<int>(TethysGame::tick()) >> 2; }
// PlaceMarker(Unit&, x, y, tipo): AMBIENT_ANIM em (x<<5, y<<5) (sub_4466B0(0x72)).
int  __fastcall T_PlaceMarker(int* unit, int x, int y, int type) {
    const int id = createMarker(portX(x) << 5, y << 5, type);
    if (!id) return 0;
    *unit = id;
    return 1;
}
// Destroço (vt off_4CF400): vt[0x98] sub_405B10 descobre p/ o jogador p.
static void wreckDiscover(int id, int p) {
    UnitRecord& w = g_units[id];
    const uint32_t old = w.wreckMask;
    w.wreckMask |= 1u << p;
    if (old == w.wreckMask || p != TethysGame::localPlayer()) return;
    postMessage(unitOX(w), unitOY(w), "Wreckage Discovered", TethysGame::tick() > 0 ? 0x97 : 0);
}
// vt[0x9C] sub_405B80: algum descobridor i aliado de p.
static bool wreckDiscovered(const UnitRecord& w, int p) {
    for (int i = 0; i < TethysGame::noPlayers(); ++i)
        if ((w.wreckMask >> i & 1u) && (g_playerData[i].allyMask >> p & 1u)) return true;
    return false;
}
// CreateWreck(x, y, tecnologia, visível): sub_4466B0(0x54, dono 6); visível →
// descoberto por todos; senão pelos jogadores de computador (+0x28 == 0).
int  __fastcall T_CreateWreck(int x, int y, int tech, int visible) {
    Unit u;
    if (!createUnit(u, static_cast<map_id>(0x54), LOCATION{portX(x), y}, 6)) return 0;
    g_units[u.id()].wreckTech = tech;                            // vt[0xA0] sub_405BE0
    for (int i = 0; i < TethysGame::noPlayers(); ++i)
        if (visible || !g_playerData[i].isHuman) wreckDiscover(u.id(), i);
    return 1;
}
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
// ?SetEMPMissile(x, y, dono, alvoX, alvoY): unidade 0x6B (ação 0x1A).
void __fastcall T_SetEMPMissile(int x, int y, int owner, int tx, int ty) {
    setEMPMissile(portX(x), y, owner, portX(tx), ty);
}
// ?SetLavaSpeed (sub_432DE0 → sub_49E240).
void __fastcall T_SetLavaSpeed(int speed) { setLavaSpeed(speed); }
// ?AddGameSound(som, jogador): sub_47F020 com máscara 1<<jogador (−1 = todos).
void __fastcall T_AddGameSound(int sound, int) { soundPlay(sound); }
// ?AddMapSound(som, LOCATION): sub_47EE00(x<<5, y<<5) — x interno.
void __fastcall T_AddMapSound(int sound, ApiLoc loc) { soundPlayAt(portX(loc.x) << 5, loc.y << 5, sound); }
// ?AddMessage(Unit, msg, jogador, som): mensagem na posição [+0x14]/[+0x18] da unidade.
// (Unit vai na pilha: ECX = msg, EDX = jogador.)
void __fastcall T_AddMessageUnit(char* msg, int toPlayer, int unit, int sound) {
    if (toPlayer != -1 && toPlayer != TethysGame::localPlayer()) return;
    if (unit <= 0 || unit >= static_cast<int>(g_units.size())) return;
    postMessage(unitOX(g_units[unit]), unitOY(g_units[unit]), msg, sound);
}
// ?SetMusicPlayList (sub_451090): a música vem de op2.clm (ausente) — não portada.
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
// ?SetVirusUL(LOCATION, spread): canto do vírus (x interno; spread em ECX).
void __fastcall M_SetVirusUL(ApiLoc loc, int spread) { setVirusUL(loc.x, loc.y, spread); }
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
int  __fastcall U_IsVehicle(int* u, void*)      { return (g_units[*u].flags & UNIT_VEHICLE) ? -1 : 0; }   // shl 1Eh; sar 1Fh
int  __fastcall U_IsLive(int* u, void*) {                                     // zera o Unit se morta
    if (*u == 0) return 0;
    const UnitRecord& r = g_units[*u];
    if (r.id != -1 && !(r.flags & UNIT_DEAD)) return 1;
    *u = 0;
    return 0;
}
void __fastcall U_DoInfect(int* u, void*)        { unitDoInfect(*u); }
int  __fastcall U_GetType(int* u, void*)         { return g_units[*u].type; }
int  __fastcall U_OwnerID(int* u, void*)         { return g_units[*u].owner & 0x0F; }   // [+0x1D] & 0xF
// isDiscovered: só destroço (0x54) → vt[0x9C](0).
int  __fastcall U_IsDiscovered(int* u, void*) {
    if (*u <= 0 || *u >= static_cast<int>(g_units.size())) return 0;
    const UnitRecord& r = g_units[*u];
    return r.type == 0x54 && wreckDiscovered(r, 0) ? 1 : 0;
}
int  __fastcall U_GetCargo(int* u, void*)        { return static_cast<int16_t>(g_units[*u].cargo); }   // movsx [+0x24]
// ?GetWeapon: o mesmo [+0x24] (a arma dos veículos armados).
int  __fastcall U_GetWeapon(int* u, void*)       { return static_cast<int16_t>(g_units[*u].cargo); }
// ?DoMove(LOCATION): viva, x em [0, larguraInterna), y em [0, altura) → pacote 2
// (mover) do dono, um ponto (sub_40E210).
void __fastcall U_DoMove(int* u, void*, ApiLoc loc) {
    if (*u <= 0 || *u >= static_cast<int>(g_units.size())) return;
    const UnitRecord& r = g_units[*u];
    if (r.id == -1 || (r.flags & UNIT_DEAD)) return;
    if (loc.x < 0 || loc.x >= g_gameMap.internalWidth() || loc.y < 0 || loc.y >= g_gameMap.height()) return;
    CommandPacket pkt;
    pkt.type = CMD_MOVE;
    pkt.unitIds.push_back(*u);
    pkt.waypoints.push_back(packLocation(portX(loc.x), loc.y));
    processCommandPacket(r.owner & 0x0F, pkt);
}
void __fastcall U_DoSelfDestruct(int* u, void*)  { unitDoSelfDestruct(*u); }
int  __fastcall U_HasOccupiedBay(int* u, void*)  { return unitHasOccupiedBay(*u); }
// ?Location: ([+0x14] >> 5, [+0x18] >> 5) em tiles INTERNOS; LOCATION por valor
// → ponteiro oculto na pilha.
ApiLoc* __fastcall U_Location(int* u, void*, ApiLoc* ret) {
    int x, y;
    unitOrigPixel(g_units[*u], x, y);
    ret->x = (x + g_gameMap.internalXOffset() * 32) >> 5;
    ret->y = y >> 5;
    return ret;
}

// ---- Enumeradores ------------------------------------------------------------------------
// InRectEnumerator (52 bytes): MAP_RECT {x1,y1,x2,y2} em tiles internos, inclusivo.
// A largura dá a volta comparando com a máscara de PIXELS (dword_54EBBC), como o original.
struct ApiRect { int x1, y1, x2, y2; };
static_assert(sizeof(MapRectIter) <= 52, "InRectEnumerator: 52 bytes");
void* __fastcall E_InRectCtor(void* self, void*, const ApiRect* rc) {
    const int pxMask = g_gameMap.internalWidth() * 32 - 1, pxW = pxMask + 1;
    int dx = rc->x2 - rc->x1;
    const int half = pxMask / 2;
    if (-half >= dx) dx += pxW;
    else if (half < dx) dx = -((-dx) & pxMask);
    const int w = dx + 1, h = rc->y2 - rc->y1 + 1;
    auto* it = new (self) MapRectIter();
    mapRectIterInit(*it, rc->x1 << 5, rc->y1 << 5, w << 5, h << 5, 0xFFFFFFFFu, 0xFFFFFFFFu);
    return self;
}
// LocationEnumerator(LOCATION&): unidades sob o ponto (x<<5, y<<5), sem filtro.
static_assert(sizeof(MapPointIter) <= 52, "LocationEnumerator");
void* __fastcall E_LocationCtor(void* self, void*, const ApiLoc* loc) {
    auto* it = new (self) MapPointIter();
    mapPointIterInit(*it, loc->x << 5, loc->y << 5, 0xFFFFFFFFu, 0xFFFFFFFFu);
    return self;
}
int __fastcall E_LocationGetNext(void* self, void*, int* unit) {
    const int id = mapPointIterNext(*static_cast<MapPointIter*>(self));
    if (!id) return 0;
    *unit = id;
    return 1;
}
int __fastcall E_InRectGetNext(void* self, void*, int* unit) {
    const int id = mapRectIterNext(*static_cast<MapRectIter*>(self));
    if (!id) return 0;
    *unit = id;
    return 1;
}

// ?PutInGarage(baia, x, y): vt[65] da garagem em (x,y) — garagem ainda não portada.
void __fastcall U_PutInGarage(int* u, void*, int bay, int x, int y) {
    std::cerr << "[Mission] PutInGarage(unidade " << *u << ", baia " << bay << ", " << x << "," << y
              << "): garagem nao portada" << std::endl;
}

// ---- Grupos (ScGroup / FightGroup) -------------------------------------------------------
// CreateFightGroup(_Player por valor): retorno oculto em ECX, _Player (12 bytes) na pilha.
int* __fastcall G_CreateFightGroup(int* ret, DllPlayer p) { *ret = aiCreateGroup(GK_FIGHT, p.playerNum); return ret; }
void __fastcall SG_AddUnits(int* self, void*, DllUnitBlock* b) {
    if (b) aiAddUnitsFromBlock(*self, b->table, b->numUnits, portX);
}
void __fastcall SG_TakeUnit(int* self, void*, int unit) { aiTakeUnit(*self, unit); }
void __fastcall SG_RemoveUnit(int* self, void*, int unit) { aiRemoveUnit(*self, unit); }
void __fastcall SG_SetLights(int* self, void*, int on) { aiSetLights(*self, on); }
void __fastcall SG_SetDeleteWhenEmpty(int* self, void*, int on) { aiSetDeleteWhenEmpty(*self, on); }
int  __fastcall SG_TotalUnitCount(int* self, void*) { return aiTotalUnitCount(*self); }
int  __fastcall SG_UnitCount(int* self, void*, int cls) { return aiUnitCount(*self, cls); }
// MAP_RECT (tiles internos) → px internos do grupo.
void __fastcall FG_SetRect(int* self, void*, int* r) { aiFightSetRect(*self, r[0], r[1], r[2], r[3]); }
void __fastcall FG_AddGuardedRect(int* self, void*, int* r) { aiFightAddGuardedRect(*self, r[0], r[1], r[2], r[3]); }
void __fastcall FG_ClearGuardedRects(int* self, void*) { aiFightClearGuardedRects(*self); }
void __fastcall FG_SetAttackType(int* self, void*, int type) { aiFightSetAttackType(*self, type); }
void __fastcall FG_SetTargetUnit(int* self, void*, int unit) { aiFightSetTargetUnit(*self, unit); }
void __fastcall FG_SetTargetGroup(int* self, void*, int grp) { aiFightSetTargetGroup(*self, grp); }
void __fastcall FG_SetFollowMode(int* self, void*, int m) { aiFightSetFollowMode(*self, m); }
void __fastcall FG_SetCombineFire(int* self, void*) { aiFightSetCombineFire(*self, true); }
void __fastcall FG_ClearCombineFire(int* self, void*) { aiFightSetCombineFire(*self, false); }
void __fastcall FG_DoGuardRect(int* self, void*) { aiFightSetMode(*self, 0); }
void __fastcall FG_DoPatrolOnly(int* self, void*) { aiFightSetMode(*self, 1); }
void __fastcall FG_DoGuardGroup(int* self, void*) { aiFightSetMode(*self, 2); }
void __fastcall FG_DoGuardUnit(int* self, void*) { aiFightSetMode(*self, 3); }
void __fastcall FG_DoAttackUnit(int* self, void*) { aiFightSetMode(*self, 4); }
void __fastcall FG_DoAttackEnemy(int* self, void*) { aiFightSetMode(*self, 5); }
void __fastcall FG_DoExitMap(int* self, void*) { aiFightSetMode(*self, 6); }
// PatrolRoute {int recomeço; LOCATION* pontos (x = −1 encerra, até 8)}.
void __fastcall FG_SetPatrolMode(int* self, void*, int* route) {
    int xs[8], ys[8], n = 0;
    const ApiLoc* pts = reinterpret_cast<const ApiLoc*>(static_cast<intptr_t>(route[1]));
    while (pts && n < 8 && pts[n].x != -1) { xs[n] = portX(pts[n].x); ys[n] = pts[n].y; ++n; }
    aiFightSetPatrol(*self, n, route[0], xs, ys);
}
void __fastcall FG_ClearPatrolMode(int* self, void*) { aiFightClearPatrol(*self); }
// _Player::SetDefaultGroup(ScGroup&) / AllyWith(j): pacote 0x32 (alterna o bit
// do remetente na máscara de aliança de j).
void __fastcall P_SetDefaultGroup(DllPlayer* self, void*, int* grp) { aiSetDefaultGroup(self->playerNum, grp ? *grp : 0xFF); }
void __fastcall P_AllyWith(DllPlayer* self, void*, int j) {
    if (j >= 0 && j < kMaxPlayers) g_playerData[j].allyMask ^= 1u << self->playerNum;
}
// Unit::DoIdle / DoUnIdle (pacotes 0x12 / 0x13), SetDamage ([+0x1E]),
// DoTransfer (pacote 0x18 → vt[0x68]).
void __fastcall U_DoIdle(int* u, void*) {
    if (*u > 0 && *u < static_cast<int>(g_units.size()) && g_units[*u].id != -1 && !(g_units[*u].flags & UNIT_DEAD))
        structureDoIdle(*u);
}
void __fastcall U_DoUnIdle(int* u, void*) {
    if (*u > 0 && *u < static_cast<int>(g_units.size()) && g_units[*u].id != -1 && !(g_units[*u].flags & UNIT_DEAD))
        structureDoActivate(*u);
}
void __fastcall U_SetDamage(int* u, void*, int dmg) {
    if (*u > 0 && *u < static_cast<int>(g_units.size())) g_units[*u].damage = static_cast<int16_t>(dmg);
}
void __fastcall U_DoTransfer(int* u, void*, int player) {
    if (*u <= 0 || *u >= static_cast<int>(g_units.size())) return;
    UnitRecord& r = g_units[*u];
    if (r.id == -1 || (r.flags & UNIT_DEAD) || player < 0 || player >= kMaxPlayers) return;
    // vt[0x68] (veículo, sub_44CB60): solta alvos, cancela 31/36/37/39, troca de lista e de dono.
    if (r.flags & UNIT_ARMED) unitResetCombat(*u);
    if (r.action == 31 || r.action == 36 || r.action == 37 || r.action == 39) {
        r.action = 0; r.field20 = 1; r.field3C = 0;
    }
    const int old = r.owner & 0x0F;
    if (r.flags & UNIT_VEHICLE) colonyVehicleRemoved(*u);
    r.owner = static_cast<uint8_t>((r.owner & 0xF0) | (player & 0x0F));
    if (r.flags & UNIT_VEHICLE) colonyVehicleAdded(*u);
    colonyMarkDirty(old);
    colonyMarkDirty(player);
}
// Triggers Rect / SpecialTarget.
int* __fastcall TR_CreateRectTrigger(int* ret, int bEnabled, int bOneShot, int player, int x, int y, int w, int h,
                                     const char* fn) {
    *ret = createRectTrigger(bEnabled, bOneShot, player, x, y, w, h, fn);
    return ret;
}
int* __fastcall TR_CreateDamagedTrigger(int* ret, int bEnabled, int bOneShot, int* group, int count, const char* fn) {
    *ret = createDamagedTrigger(bEnabled, bOneShot, group ? *group : 0xFF, count, fn);
    return ret;
}
int* __fastcall TR_CreatePointTrigger(int* ret, int bEnabled, int bOneShot, int player, int x, int y, const char* fn) {
    *ret = createPointTrigger(bEnabled, bOneShot, player, x, y, fn);
    return ret;
}
int* __fastcall TR_CreateOnePlayerLeftTrigger(int* ret, int bEnabled, int bOneShot, const char* fn) {
    *ret = createOnePlayerLeftTrigger(bEnabled, bOneShot, fn);
    return ret;
}
int* __fastcall TR_CreateSpecialTarget(int* ret, int bEnabled, int bOneShot, int* target, int sourceType, const char* fn) {
    *ret = createSpecialTargetTrigger(bEnabled, bOneShot, target ? *target : 0, sourceType, fn);
    return ret;
}
void __fastcall TR_GetSpecialTargetData(int* trig, int* unit) {
    if (unit) *unit = trig ? specialTargetSource(*trig) : -1;
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
int  __fastcall SS_IsInitialized(int* stub, void*) { return *stub != 0xFF ? 1 : 0; }
// Trigger::Enable / Disable: vt[3] / vt[4] do objeto do stub (dword_56B71C[idx]).
void __fastcall TRG_Enable(int* stub, void*)  { stubEnable(*stub, true); }
void __fastcall TRG_Disable(int* stub, void*) { stubEnable(*stub, false); }

// PlayerVehicleEnum (4 bytes no objeto da DLL: só o cursor) e
// PlayerBuildingEnum (8 bytes: cursor + tipo; 0 = todos). As listas do jogador
// (dword_56EEF8 veículos, dword_56EEF4 prédios) viram índice*8 + jogador.
void* __fastcall E_PlayerVehicleCtor(void* self, void*, int player) {
    *static_cast<int*>(self) = player & 7;
    return self;
}
int __fastcall E_PlayerVehicleGetNext(void* self, void*, int* unit) {
    int& cur = *static_cast<int*>(self);
    const std::vector<int>& L = colonyVehicles(cur & 7);
    const int pos = cur >> 3;
    if (pos >= static_cast<int>(L.size())) return 0;
    *unit = L[pos];
    cur += 8;
    return 1;
}
void* __fastcall E_PlayerBuildingCtor(void* self, void*, int player, int type) {
    int* e = static_cast<int*>(self);
    e[0] = player & 7;
    e[1] = type;
    return self;
}
int __fastcall E_PlayerBuildingGetNext(void* self, void*, int* unit) {
    int* e = static_cast<int*>(self);
    const std::vector<int>& L = colonyStructures(e[0] & 7);
    for (int pos = e[0] >> 3; pos < static_cast<int>(L.size()); ++pos) {
        if (e[1] != 0 && g_units[L[pos]].type != e[1]) continue;
        *unit = L[pos];
        e[0] = ((pos + 1) << 3) | (e[0] & 7);
        return 1;
    }
    e[0] = (static_cast<int>(L.size()) << 3) | (e[0] & 7);
    return 0;
}

// PlayerUnitEnum(jogador): lista de veículos (+0xC1C), na ordem.
struct ApiPlayerEnum { int player; int pos; };
void* __fastcall E_PlayerUnitCtor(void* self, void*, int player) {
    auto* e = static_cast<ApiPlayerEnum*>(self);
    e->player = player;
    e->pos = 0;
    return self;
}
int __fastcall E_PlayerUnitGetNext(void* self, void*, int* unit) {
    auto* e = static_cast<ApiPlayerEnum*>(self);
    const std::vector<int>& L = colonyVehicles(e->player);
    if (e->pos >= static_cast<int>(L.size())) return 0;
    *unit = L[e->pos++];
    return 1;
}

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
// ?CreateTimeTrigger(min, max): tempo = min + GetRand(max − min).
int* __fastcall TR_CreateTimeTriggerRange(int* ret, int bEnabled, int bOneShot, int tmin, int tmax, const char* fn) {
    const int time = tmin + TethysGame::getRand(tmax - tmin);
    *ret = createTimeTrigger(bEnabled, bOneShot, time, fn);
    return ret;
}
int* __fastcall TR_CreateResourceTrigger(int* ret, int bEnabled, int bOneShot, int res, int amount, int player,
                                         int compare, const char* fn) {
    *ret = createResourceTrigger(bEnabled, bOneShot, res, amount, player, compare, fn);
    return ret;
}
int* __fastcall TR_CreateEvacTrigger(int* ret, int bEnabled, int bOneShot, int player, const char* fn) {
    *ret = createEvacTrigger(bEnabled, bOneShot, player, fn);
    return ret;
}
int* __fastcall TR_CreateOperationalTrigger(int* ret, int bEnabled, int bOneShot, int player, int type, int count,
                                            int compare, const char* fn) {
    *ret = createOperationalTrigger(bEnabled, bOneShot, player, type, count, compare, fn);
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
        {"?Ore@_Player@@QBEHXZ", reinterpret_cast<void*>(&P_Ore)},
        {"?Kids@_Player@@QBEHXZ", reinterpret_cast<void*>(&P_Kids)},
        {"?Workers@_Player@@QBEHXZ", reinterpret_cast<void*>(&P_Workers)},
        {"?Scientists@_Player@@QBEHXZ", reinterpret_cast<void*>(&P_Scientists)},
        {"?HasTechnology@_Player@@QBEHH@Z", reinterpret_cast<void*>(&P_HasTechnology)},
        {"?resetChecks@_Player@@QAEXXZ", reinterpret_cast<void*>(&P_ResetChecks)},
        {"?canBuildBuilding@_Player@@QAEHXZ", reinterpret_cast<void*>(&P_CanBuildBuilding)},
        {"?canBuildVehicle@_Player@@QAEHH@Z", reinterpret_cast<void*>(&P_CanBuildVehicle)},
        {"?canAccumulateOre@_Player@@QAEHXZ", reinterpret_cast<void*>(&P_CanAccumulateOre)},
        {"?canDoResearch@_Player@@QAEHH@Z", reinterpret_cast<void*>(&P_CanDoResearch)},
        {"?FreeMoraleLevel@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_FreeMoraleLevel)},
        {"?SetMicrobeSpreadSpeed@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_SetMicrobeSpreadSpeed)},
        {"?SetVirusUL@GameMap@@SIXULOCATION@@H@Z", reinterpret_cast<void*>(&M_SetVirusUL)},
        {"?GetType@Unit@@QBE?AW4map_id@@XZ", reinterpret_cast<void*>(&U_GetType)},
        {"?GetCargo@Unit@@QBE?AW4map_id@@XZ", reinterpret_cast<void*>(&U_GetCargo)},
        {"?IsInitialized@ScStub@@QAEHXZ", reinterpret_cast<void*>(&SS_IsInitialized)},
        {"??0PlayerUnitEnum@@QAE@H@Z", reinterpret_cast<void*>(&E_PlayerUnitCtor)},
        {"?GetNext@PlayerUnitEnum@@QAEHAAVUnit@@@Z", reinterpret_cast<void*>(&E_PlayerUnitGetNext)},
        {"?CreateTimeTrigger@@YI?AVTrigger@@HHHHPBD@Z", reinterpret_cast<void*>(&TR_CreateTimeTriggerRange)},
        {"?CreateResourceTrigger@@YI?AVTrigger@@HHW4trig_res@@HHW4compare_mode@@PBD@Z", reinterpret_cast<void*>(&TR_CreateResourceTrigger)},
        {"?CreateEvacTrigger@@YI?AVTrigger@@HHHPBD@Z", reinterpret_cast<void*>(&TR_CreateEvacTrigger)},
        {"?CreateOperationalTrigger@@YI?AVTrigger@@HHHW4map_id@@HW4compare_mode@@PBD@Z", reinterpret_cast<void*>(&TR_CreateOperationalTrigger)},
        {"?ForceMoraleGreat@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleGreat)},
        {"?ForceMoraleOK@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleOK)},
        {"?ForceMoralePoor@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoralePoor)},
        {"?ForceMoraleRotten@TethysGame@@SIXH@Z", reinterpret_cast<void*>(&T_ForceMoraleRotten)},
        {"?AddMessage@TethysGame@@SIXHHPADHH@Z", reinterpret_cast<void*>(&T_AddMessage)},
        {"?SetCargo@Unit@@QAEXW4map_id@@0@Z", reinterpret_cast<void*>(&U_SetCargo)},
        {"?PutInGarage@Unit@@QAEXHHH@Z", reinterpret_cast<void*>(&U_PutInGarage)},
        {"?IsVehicle@Unit@@QBEHXZ", reinterpret_cast<void*>(&U_IsVehicle)},
        {"?IsLive@Unit@@QAEHXZ", reinterpret_cast<void*>(&U_IsLive)},
        {"?DoInfect@Unit@@QAEXXZ", reinterpret_cast<void*>(&U_DoInfect)},
        {"?DoSelfDestruct@Unit@@QAEXXZ", reinterpret_cast<void*>(&U_DoSelfDestruct)},
        {"?HasOccupiedBay@Unit@@QBEHXZ", reinterpret_cast<void*>(&U_HasOccupiedBay)},
        {"?Location@Unit@@QBE?AULOCATION@@XZ", reinterpret_cast<void*>(&U_Location)},
        {"??0InRectEnumerator@@QAE@ABUMAP_RECT@@@Z", reinterpret_cast<void*>(&E_InRectCtor)},
        {"?GetNext@InRectEnumerator@@QAEHAAVUnit@@@Z", reinterpret_cast<void*>(&E_InRectGetNext)},
        {"??0ScStub@@QAE@XZ", reinterpret_cast<void*>(&SS_Ctor)},
        {"??0ScGroup@@QAE@XZ", reinterpret_cast<void*>(&SS_Ctor)},
        {"??0Trigger@@QAE@XZ", reinterpret_cast<void*>(&SS_Ctor)},
        {"?Enable@Trigger@@QAEXXZ", reinterpret_cast<void*>(&TRG_Enable)},
        {"?Disable@Trigger@@QAEXXZ", reinterpret_cast<void*>(&TRG_Disable)},
        {"??0PlayerVehicleEnum@@QAE@H@Z", reinterpret_cast<void*>(&E_PlayerVehicleCtor)},
        {"?GetNext@PlayerVehicleEnum@@QAEHAAVUnit@@@Z", reinterpret_cast<void*>(&E_PlayerVehicleGetNext)},
        {"??0PlayerBuildingEnum@@QAE@HW4map_id@@@Z", reinterpret_cast<void*>(&E_PlayerBuildingCtor)},
        {"?GetNext@PlayerBuildingEnum@@QAEHAAVUnit@@@Z", reinterpret_cast<void*>(&E_PlayerBuildingGetNext)},
        {"?canAccumulateRareOre@_Player@@QAEHXZ", reinterpret_cast<void*>(&P_CanAccumulateRareOre)},
        {"?canBuildSpace@_Player@@QAEHXZ", reinterpret_cast<void*>(&P_CanBuildSpace)},
        {"?RareOre@_Player@@QBEHXZ", reinterpret_cast<void*>(&P_RareOre)},
        {"?GetDefaultGroup@_Player@@QAE?AVScGroup@@XZ", reinterpret_cast<void*>(&P_GetDefaultGroup)},
        {"?Tick@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_Tick)},
        {"?Time@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_Time)},
        {"?PlaceMarker@TethysGame@@SIHAAVUnit@@HHH@Z", reinterpret_cast<void*>(&T_PlaceMarker)},
        {"?CreateWreck@TethysGame@@SIHHHW4map_id@@H@Z", reinterpret_cast<void*>(&T_CreateWreck)},
        {"?OwnerID@Unit@@QBEHXZ", reinterpret_cast<void*>(&U_OwnerID)},
        {"?IsEden@_Player@@QAEHXZ", reinterpret_cast<void*>(&P_IsEden)},
        {"?NoPlayers@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_NoPlayers)},
        {"?CanHaveDisasters@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_CanHaveDisasters)},
        {"?UsesDayNight@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_UsesDayNight)},
        {"?UsesMorale@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_UsesMorale)},
        {"?InitialUnits@TethysGame@@SIHXZ", reinterpret_cast<void*>(&T_InitialUnits)},
        {"?FindEMPMissleTarget@TethysGame@@SI?AULOCATION@@HHHHH@Z", reinterpret_cast<void*>(&T_FindEMPMissleTarget)},
        {"?GetWeapon@Unit@@QBE?AW4map_id@@XZ", reinterpret_cast<void*>(&U_GetWeapon)},
        {"?SetEMPMissile@TethysGame@@SIXHHHHH@Z", reinterpret_cast<void*>(&T_SetEMPMissile)},
        {"?DoMove@Unit@@QAEXULOCATION@@@Z", reinterpret_cast<void*>(&U_DoMove)},
        {"??0LocationEnumerator@@QAE@ABULOCATION@@@Z", reinterpret_cast<void*>(&E_LocationCtor)},
        {"?GetNext@LocationEnumerator@@QAEHAAVUnit@@@Z", reinterpret_cast<void*>(&E_LocationGetNext)},
        {"??0FightGroup@@QAE@XZ", reinterpret_cast<void*>(&SS_Ctor)},
        {"?CreateDamagedTrigger@@YI?AVTrigger@@HHAAVScGroup@@HPBD@Z", reinterpret_cast<void*>(&TR_CreateDamagedTrigger)},
        {"?CreatePointTrigger@@YI?AVTrigger@@HHHHHPBD@Z", reinterpret_cast<void*>(&TR_CreatePointTrigger)},
        {"?CreateOnePlayerLeftTrigger@@YI?AVTrigger@@HHPBD@Z", reinterpret_cast<void*>(&TR_CreateOnePlayerLeftTrigger)},
        {"?isDiscovered@Unit@@QBEHXZ", reinterpret_cast<void*>(&U_IsDiscovered)},
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
        {"?AddMessage@TethysGame@@SIXVUnit@@PADHH@Z", reinterpret_cast<void*>(&T_AddMessageUnit)},
        {"?CreateFightGroup@@YI?AVFightGroup@@V_Player@@@Z", reinterpret_cast<void*>(&G_CreateFightGroup)},
        {"?AddUnits@ScGroup@@QAEXAAVUnitBlock@@@Z", reinterpret_cast<void*>(&SG_AddUnits)},
        {"?TakeUnit@ScGroup@@QAEXVUnit@@@Z", reinterpret_cast<void*>(&SG_TakeUnit)},
        {"?RemoveUnit@ScGroup@@QAEXVUnit@@@Z", reinterpret_cast<void*>(&SG_RemoveUnit)},
        {"?SetLights@ScGroup@@QAEXH@Z", reinterpret_cast<void*>(&SG_SetLights)},
        {"?SetDeleteWhenEmpty@ScGroup@@QAEXH@Z", reinterpret_cast<void*>(&SG_SetDeleteWhenEmpty)},
        {"?TotalUnitCount@ScGroup@@QAEHXZ", reinterpret_cast<void*>(&SG_TotalUnitCount)},
        {"?UnitCount@ScGroup@@QAEHW4UnitClassifactions@@@Z", reinterpret_cast<void*>(&SG_UnitCount)},
        {"?SetRect@FightGroup@@QAEXAAUMAP_RECT@@@Z", reinterpret_cast<void*>(&FG_SetRect)},
        {"?AddGuardedRect@FightGroup@@QAEXAAUMAP_RECT@@@Z", reinterpret_cast<void*>(&FG_AddGuardedRect)},
        {"?ClearGuarderdRects@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_ClearGuardedRects)},
        {"?SetAttackType@FightGroup@@QAEXW4map_id@@@Z", reinterpret_cast<void*>(&FG_SetAttackType)},
        {"?SetTargetUnit@FightGroup@@QAEXVUnit@@@Z", reinterpret_cast<void*>(&FG_SetTargetUnit)},
        {"?SetTargetGroup@FightGroup@@QAEXVScGroup@@@Z", reinterpret_cast<void*>(&FG_SetTargetGroup)},
        {"?SetFollowMode@FightGroup@@QAEXH@Z", reinterpret_cast<void*>(&FG_SetFollowMode)},
        {"?SetCombineFire@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_SetCombineFire)},
        {"?ClearCombineFire@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_ClearCombineFire)},
        {"?DoGuardRect@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoGuardRect)},
        {"?DoPatrolOnly@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoPatrolOnly)},
        {"?DoGuardGroup@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoGuardGroup)},
        {"?DoGuardUnit@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoGuardUnit)},
        {"?DoAttackUnit@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoAttackUnit)},
        {"?DoAttackEnemy@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoAttackEnemy)},
        {"?DoExitMap@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_DoExitMap)},
        {"?SetPatrolMode@FightGroup@@QAEXAAUPatrolRoute@@@Z", reinterpret_cast<void*>(&FG_SetPatrolMode)},
        {"?ClearPatrolMode@FightGroup@@QAEXXZ", reinterpret_cast<void*>(&FG_ClearPatrolMode)},
        {"?SetDefaultGroup@_Player@@QAEXAAVScGroup@@@Z", reinterpret_cast<void*>(&P_SetDefaultGroup)},
        {"?AllyWith@_Player@@QAEXH@Z", reinterpret_cast<void*>(&P_AllyWith)},
        {"?DoIdle@Unit@@QAEXXZ", reinterpret_cast<void*>(&U_DoIdle)},
        {"?DoUnIdle@Unit@@QAEXXZ", reinterpret_cast<void*>(&U_DoUnIdle)},
        {"?SetDamage@Unit@@QAEXH@Z", reinterpret_cast<void*>(&U_SetDamage)},
        {"?DoTransfer@Unit@@QAEXH@Z", reinterpret_cast<void*>(&U_DoTransfer)},
        {"?CreateRectTrigger@@YI?AVTrigger@@HHHHHHHPBD@Z", reinterpret_cast<void*>(&TR_CreateRectTrigger)},
        {"?CreateSpecialTarget@@YI?AVTrigger@@HHAAVUnit@@W4map_id@@PBD@Z", reinterpret_cast<void*>(&TR_CreateSpecialTarget)},
        {"?GetSpecialTargetData@@YIXAAVTrigger@@AAVUnit@@@Z", reinterpret_cast<void*>(&TR_GetSpecialTargetData)},
        {"?AddGameSound@TethysGame@@SIXHH@Z", reinterpret_cast<void*>(&T_AddGameSound)},
        {"?AddMapSound@TethysGame@@SIXHULOCATION@@@Z", reinterpret_cast<void*>(&T_AddMapSound)},
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
