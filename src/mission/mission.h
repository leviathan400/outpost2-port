#pragma once

#include <string>

#include "pe_loader.h"

namespace op2 {

// DescBlock exportado pela DLL da missão (AIModDesc): tipo da missão (−3
// tutorial, −1 colônia, −4..−8 multiplayer, >0 campanha), nº de jogadores,
// nível máximo de tecnologia e "missão só de unidades".
struct AIModDesc {
    int missionType;
    int numPlayers;
    int maxTechLevel;
    int boolUnitMission;
    const char* mapName;
    const char* levelDesc;
    const char* techtreeName;
    int checksum;
};

// Script da missão — porta do objeto dword_4EFCD8 (sub_402A10 carrega a DLL e
// resolve InitProc [+4], AIProc [+8], StatusProc [+0xC], GetSaveRegions e
// DescBlock [+0x4C]; sub_402E10 chama o InitProc; sub_4034B0 resolve os nomes
// de função dos triggers).
struct MissionScript {
    PeModule module;
    int  (*initProc)() = nullptr;
    void (*aiProc)() = nullptr;
    int  (*statusProc)() = nullptr;
    const AIModDesc* desc = nullptr;
    void* saveBuffer = nullptr;   // +0x10 (GetSaveRegions)
    int   saveLength = 0;         // +0x14
    std::string dllName, mapName, levelDesc, techtreeName;

    bool load(const std::string& dll, std::string& error);
    void unload();
    void* proc(const std::string& name) const { return module.proc(name); }
};

// Missão em andamento (dword_4EFCD8), ou nullptr.
MissionScript* currentMission();
void setCurrentMission(MissionScript* m);

// Resolução dos imports "Outpost2.exe" das DLLs (src/mission/op2_api.cpp).
void* op2ApiResolve(const std::string& mangledName);

} // namespace op2
