#include "mission.h"

#include <cctype>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace op2 {

static MissionScript* s_current = nullptr;
MissionScript* currentMission() { return s_current; }
void setCurrentMission(MissionScript* m) { s_current = m; }

static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool MissionScript::load(const std::string& dll, std::string& error) {
    unload();
    auto resolve = [](const std::string& lib, const std::string& name) -> void* {
        if (lower(lib) == "outpost2.exe") {
            void* p = op2ApiResolve(name);
            if (!p) std::cerr << "[Mission] API nao portada: " << name << std::endl;
            return p;
        }
        HMODULE h = LoadLibraryA(lib.c_str());
        if (!h) return nullptr;
        if (!name.empty() && name[0] == '#')
            return reinterpret_cast<void*>(GetProcAddress(h, MAKEINTRESOURCEA(std::stoi(name.substr(1)))));
        return reinterpret_cast<void*>(GetProcAddress(h, name.c_str()));
    };
    if (!module.load(dll, resolve, error)) return false;
    dllName = dll;
    initProc = reinterpret_cast<int (*)()>(module.proc("InitProc"));
    aiProc = reinterpret_cast<void (*)()>(module.proc("AIProc"));
    statusProc = reinterpret_cast<int (*)()>(module.proc("StatusProc"));
    desc = static_cast<const AIModDesc*>(module.proc("DescBlock"));
    if (!desc) { error = "DescBlock ausente"; unload(); return false; }
    // sub_402A10: GetSaveRegions(&this->+0x10) = {buffer, tamanho} da memória
    // da DLL que vai no jogo salvo; sem o export, nada (+0x10 = 0).
    saveBuffer = nullptr;
    saveLength = 0;
    if (auto gsr = reinterpret_cast<void (__cdecl*)(void*)>(module.proc("GetSaveRegions"))) {
        struct { void* buf; int len; } bd = {nullptr, 0};
        gsr(&bd);
        saveBuffer = bd.buf;
        saveLength = bd.len;
    }
    if (const char* m = static_cast<const char*>(module.proc("MapName"))) mapName = m;
    if (const char* l = static_cast<const char*>(module.proc("LevelDesc"))) levelDesc = l;
    if (const char* t = static_cast<const char*>(module.proc("TechtreeName"))) techtreeName = t;
    return true;
}

void MissionScript::unload() {
    if (s_current == this) s_current = nullptr;
    module.unload();
    initProc = nullptr; aiProc = nullptr; statusProc = nullptr; desc = nullptr;
    dllName.clear(); mapName.clear(); levelDesc.clear(); techtreeName.clear();
}

} // namespace op2
