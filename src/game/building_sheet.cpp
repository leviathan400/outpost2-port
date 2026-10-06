#include "building_sheet.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../res/res_manager.h"
#include "structure_anim.h"
#include "vehicle_anim.h"

namespace op2 {

static StructSheetDesc s_desc[0x3B];
static bool s_loaded = false;

const StructSheetDesc* structSheet(int type) {
    if (type < 0x15 || type > 0x3A || !s_desc[type].loaded) return nullptr;
    return &s_desc[type];
}

// Tokenizador de coluna (sub_4112F0): separador = TAB; tira a aspa inicial, a
// aspa final vira espaço; valor numérico = atoi(texto).
struct SheetCol { std::string text; int value = 0; };

static std::vector<SheetCol> splitRow(const std::string& line) {
    std::vector<SheetCol> cols;
    size_t p = 0;
    while (true) {
        size_t q = line.find('\t', p);
        std::string t = line.substr(p, q == std::string::npos ? std::string::npos : q - p);
        if (!t.empty() && t.front() == '"') t.erase(0, 1);
        if (!t.empty() && t.back() == '"') t.back() = ' ';
        SheetCol c; c.text = t; c.value = std::atoi(t.c_str());
        cols.push_back(std::move(c));
        if (q == std::string::npos) break;
        p = q + 1;
    }
    return cols;
}

// Colunas Y/N (switch sobre char-'0'): '1','Y','y' → 1; o resto → 0.
static int yesNo(const SheetCol& c) {
    if (c.text.empty()) return 0;
    char ch = c.text[0];
    return (ch == '1' || ch == 'Y' || ch == 'y') ? 1 : 0;
}

// Lê uma planilha do SHEETS.VOL em linhas (sem \r).
static bool readSheetLines(const char* name, std::vector<std::string>& lines) {
    auto stream = g_ResManager.openStream(name);
    if (!stream) {
        std::cerr << "[Sheets] " << name << " nao encontrado" << std::endl;
        return false;
    }
    std::string text(reinterpret_cast<const char*>(stream->data()), stream->size());
    size_t p = 0;
    while (p < text.size()) {
        size_t q = text.find('\n', p);
        std::string l = text.substr(p, q == std::string::npos ? std::string::npos : q - p);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(std::move(l));
        if (q == std::string::npos) break;
        p = q + 1;
    }
    return true;
}

static VehPlayerStats s_veh[0x11][7];
static bool s_vehLoaded = false;

const VehPlayerStats* vehicleSheet(int type, int player) {
    if (type < 1 || type > 0x10 || player < 0 || player > 6) return nullptr;
    return &s_veh[type][player];
}

bool loadVehicleSheet() {
    if (s_vehLoaded) return true;
    std::vector<std::string> lines;
    if (!readSheetLines("vehicles.txt", lines)) return false;
    for (size_t li = 2; li < lines.size(); ++li) {
        std::vector<SheetCol> c = splitRow(lines[li]);
        if (c.size() < 18 || c[0].text.empty()) continue;
        int type = -1;
        for (int t = 1; t <= 0x10; ++t) {
            const char* n = vehicleCodeName(t);
            if (n && c[0].text == n) { type = t; break; }
        }
        if (type < 0) continue;   // ex. SCORPION_PACK: sem descritor próprio
        VehPlayerStats& s = s_veh[type][0];
        s.hitPoints = c[4].value;        // +0x00
        s.buildTime = c[10].value;       // +0x14 (desc+0x1C)
        s.productionRate = c[17].value;  // +0x2C (desc+0x34)
    }
    for (int t = 1; t <= 0x10; ++t)
        for (int pl = 1; pl < 7; ++pl) s_veh[t][pl] = s_veh[t][0];
    s_vehLoaded = true;
    return true;
}

bool loadBuildingSheet() {
    if (s_loaded) return true;
    std::vector<std::string> lines;
    if (!readSheetLines("building.txt", lines)) return false;
    // 2 linhas de cabeçalho; a 3ª leitura (sub_411220) já é a 1ª linha de dados.
    for (size_t li = 2; li < lines.size(); ++li) {
        std::vector<SheetCol> c = splitRow(lines[li]);
        if (c.size() < 32 || c[0].text.empty()) continue;
        // Coluna 0: Code_Name → descritor pelo GetName (loc_445127).
        int type = -1;
        for (int t = 0x15; t <= 0x3A; ++t)
            if (c[0].text == structureCodeName(t)) { type = t; break; }
        if (type < 0) continue;                      // loc_44563F: linha ignorada
        StructSheetDesc& d = s_desc[type];
        StructPlayerStats& s = d.player[0];
        // col 1 Produce List Name (+0x220), col 2 Building Name (+0x1F8, vem de
        // uma tabela fixa de nomes) — texto, não usados aqui.
        d.sizeX = c[3].value;                                      // +0x248
        d.sizeY = c[4].value;                                      // +0x24A
        switch (c[5].text.empty() ? 0 : c[5].text[0]) {           // +0x1F4
            case 'B': case 'b': d.ownerFlags |= 0x800 | 0x1000; break;
            case 'E': case 'e': d.ownerFlags |= 0x1000; break;
            case 'P': case 'p': d.ownerFlags |= 0x800; break;
            default: break;
        }
        s.hitPoints = c[6].value;                                  // +0x08
        s.armor = c[7].value;          // (o engine guarda dword_4E1510[classe])
        d.flags24C = (d.flags24C & ~1u) | yesNo(c[8]);             // Border
        s.commonOre = c[9].value;                                  // +0x14
        s.rareOre = c[10].value;                                   // +0x18
        d.flags24C = (d.flags24C & ~2u) | (yesNo(c[11]) << 1);     // Structure Kit
        s.buildTime = c[12].value;                                 // +0x1C
        d.commonRubble = static_cast<uint8_t>(c[13].value);        // +0x2A3
        d.rareRubble = static_cast<uint8_t>(c[14].value);          // +0x2A1
        s.power = c[15].value;                                     // +0x2C
        s.workers = c[16].value;                                   // +0x30
        s.scientists = c[17].value;                                // +0x34
        s.sight = c[18].value;                                     // +0x20
        for (int k = 0; k < 4; ++k)                                // Docking bits 2..5
            d.flags24C = (d.flags24C & ~(4u << k)) | (yesNo(c[19 + k]) << (2 + k));
        d.dockLocE = static_cast<uint8_t>(c[23].value);            // +0x2A4
        d.dockLocP = static_cast<uint8_t>(c[24].value);            // +0x2A5
        s.storageCapacity = c[25].value;                           // +0x38
        s.productionCapacity = c[26].value;                        // +0x3C
        s.storageBays = c[27].value;                               // +0x40
        d.flags24C = (d.flags24C & ~0x40u) | (yesNo(c[28]) << 6);  // AutoTargeted
        d.flags24C = (d.flags24C & ~0x180u) | ((static_cast<uint32_t>(c[29].value) << 7) & 0x180u);
        d.resourcePriority = static_cast<uint8_t>(c[30].value);    // +0x2A0
        d.researchTopic = c[31].value;                             // +0x1E4
        d.loaded = true;
    }
    // loc_445EDA: bloco do jogador 0 copiado p/ os jogadores 1..6.
    for (int t = 0x15; t <= 0x3A; ++t)
        for (int pl = 1; pl < 7; ++pl) s_desc[t].player[pl] = s_desc[t].player[0];
    s_loaded = true;
    return true;
}

} // namespace op2
