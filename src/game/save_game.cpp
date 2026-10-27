#include "save_game.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>

#include "colony.h"
#include "player.h"
#include "save_stream.h"
#include "tethys_game.h"
#include "unit.h"
#include "../mission/mission.h"

namespace op2 {

namespace {

// unk_4D6378 (0x19 bytes).
const char kSignature[0x19] = "OUTPOST 2.00 SAVED GAME\x1A";
const uint32_t kFormat = 1;
const uint32_t kScriptMagic = 0xAEDE0034u;       // sub_402B50

// Assinatura do layout das estruturas do port: um jogo salvo de outra versão
// do executável é recusado em vez de ler lixo.
uint32_t layoutSignature() {
    return static_cast<uint32_t>(sizeof(UnitRecord)) * 7u + static_cast<uint32_t>(sizeof(PlayerData)) * 131u +
           static_cast<uint32_t>(sizeof(ColonyState)) * 977u;
}

void headerIo(SaveStream& s, SaveHeader& h) {
    s.magic(0x31524448u);                         // "HDR1"
    uint32_t fmt = kFormat, sig = layoutSignature();
    s.io(fmt);
    s.io(sig);
    if (s.loading() && (fmt != kFormat || sig != layoutSignature())) { s.fail(); return; }
    s.ioStr(h.description);
    s.ioStr(h.dll);
    s.io(h.time);
    s.io(h.tick);
}

// sub_402B50 / sub_402CB0: nome da DLL e a região de memória do GetSaveRegions.
void scriptIo(SaveStream& s) {
    s.magic(kScriptMagic);
    MissionScript* m = currentMission();
    std::string name = m ? m->dllName : "";
    s.ioStr(name);
    int32_t len = m ? m->saveLength : 0;
    s.io(len);
    if (len > 0) {
        if (!m || !m->saveBuffer || len != m->saveLength) {
            if (s.loading()) { s.fail(); return; }
        } else {
            s.raw(m->saveBuffer, static_cast<size_t>(len));
        }
    }
    s.magic(kScriptMagic);
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool checkSignature(SaveStream& s) {
    char sig[0x19];
    s.raw(sig, sizeof(sig));
    return s.ok() && std::memcmp(sig, kSignature, sizeof(sig)) == 0;
}

} // namespace

std::string saveSlotPath(int slot) {
    char b[32];
    std::snprintf(b, sizeof(b), "SGAME%d.OP2", slot);
    return b;
}

bool saveReadHeader(const std::string& path, SaveHeader& out) {
    SaveStream s(true);
    std::vector<uint8_t> data;
    if (!readFile(path, data)) return false;
    s.setBuffer(std::move(data));
    if (!checkSignature(s)) return false;
    headerIo(s, out);
    return s.ok();
}

bool saveGameWrite(const std::string& path, const std::string& description,
                   const std::vector<uint8_t>& minimap, const std::vector<uint8_t>& extra) {
    SaveStream s(false);
    char sig[0x19];
    std::memcpy(sig, kSignature, sizeof(sig));
    s.raw(sig, sizeof(sig));
    SaveHeader h;
    h.description = description;
    h.dll = currentMission() ? currentMission()->dllName : "";
    h.time = static_cast<int64_t>(std::time(nullptr));
    h.tick = TethysGame::tick();
    headerIo(s, h);
    // Ordem de sub_48A120.
    s.magic(0x54454853u); sheetsSerialize(s);        // sub_446670 (planilhas por jogador)
    s.magic(0x2050414Du); mapSerialize(s);           // sub_4356F0
    s.magic(0x54494E55u); unitsSerialize(s);         // sub_4391E0
    s.magic(0x53485445u); tethysSerialize(s);        // TethysGame (0x484 bytes)
    scriptIo(s);                                     // sub_402B50
    s.magic(0x52594C50u); playersSerialize(s); dllPlayersSerialize(s); colonySerialize(s);   // sub_490220 ×7
    s.magic(0x48434552u); researchSerialize(s);      // sub_4734C0
    std::vector<uint8_t> mm = minimap;
    s.ioVec(mm);                                     // sub_420580 (minimapa/globo)
    s.magic(0x4753534Du); messagesSerialize(s);      // mensagens (so do port)
    s.magic(0x42555453u); triggersSerialize(s);      // sub_47B6C0
    s.magic(0x41525458u);                            // sub_4083B0 (interface)
    std::vector<uint8_t> ui = extra;
    s.ioVec(ui);
    s.magic(0x41564144u); disasterSerialize(s);      // sub_49E820 (lava, micróbio)
    s.magic(0x21444E45u);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(s.buffer().data()), static_cast<std::streamsize>(s.buffer().size()));
    std::cout << "[Save] " << path << ": " << s.buffer().size() << " bytes (tick " << h.tick << ")" << std::endl;
    return static_cast<bool>(f);
}

bool saveGameRead(const std::string& path, std::vector<uint8_t>& minimap,
                  std::vector<uint8_t>& extra) {
    SaveStream s(true);
    std::vector<uint8_t> data;
    if (!readFile(path, data)) return false;
    s.setBuffer(std::move(data));
    if (!checkSignature(s)) return false;
    SaveHeader h;
    headerIo(s, h);
    auto step = [&](const char* what) {
        if (!s.ok()) std::cerr << "[Load] falhou na secao " << what << std::endl;
        return s.ok();
    };
    s.magic(0x54454853u); sheetsSerialize(s);        if (!step("planilhas")) return false;
    s.magic(0x2050414Du); mapSerialize(s);           if (!step("mapa")) return false;
    s.magic(0x54494E55u); unitsSerialize(s);         if (!step("unidades")) return false;
    s.magic(0x53485445u); tethysSerialize(s);        if (!step("TethysGame")) return false;
    scriptIo(s);                                     if (!step("script")) return false;
    s.magic(0x52594C50u); playersSerialize(s); dllPlayersSerialize(s); colonySerialize(s);
    if (!step("jogadores")) return false;
    s.magic(0x48434552u); researchSerialize(s);      if (!step("pesquisa")) return false;
    s.ioVec(minimap);                                if (!step("minimapa")) return false;
    s.magic(0x4753534Du); messagesSerialize(s);      if (!step("mensagens")) return false;
    s.magic(0x42555453u); triggersSerialize(s);      if (!step("stubs")) return false;
    s.magic(0x41525458u); s.ioVec(extra);            if (!step("interface")) return false;
    s.magic(0x41564144u); disasterSerialize(s);      if (!step("lava")) return false;
    s.magic(0x21444E45u);
    return step("fim");
}

} // namespace op2
