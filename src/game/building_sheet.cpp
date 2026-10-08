#include "building_sheet.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../res/res_manager.h"
#include "structure_anim.h"
#include "vehicle_anim.h"
#include "unit_names.h"

namespace op2 {

static StructSheetDesc s_desc[0x3B];

// Nomes de exibição: sub_445070 copia p/ desc+0x1F8 um nome de uma tabela FIXA
// do EXE, avançando uma entrada por LINHA de dados da planilha (prédios:
// [0x4E94E8 - 0xA8] = 0x4E9440; veículos: 0x4E93FC). Extraídas do OUTPOST2.EXE.
static const char* const kBuildingRowNames[] = {
    "Command Center", "Tokamak", "MHD Generator", "Geothermal Plant", "Solar Power Array",
    "Structure Factory", "Guard Post", "Common Ore Smelter", "Common Storage", "Advanced Lab",
    "Standard Lab", "Basic Lab", "Rare Ore Smelter", "Rare Storage", "Vehicle Factory",
    "University", "Nursery", "Agridome", "Robot Command Center", "Spaceport",
    "Medical Center", "Arachnid Factory", "DIRT", "Garage", "GORF",
    "Advanced Residence", "Residence", "Reinforced Residence", "Observatory", "Meteor Defense",
    "Recreation Facility", "Consumer Factory", "Forum", "Trade Center", "Light Tower",
    "Magma Well", "Common Ore Mine", "Rare Ore Mine",
};
static const char* const kVehicleRowNames[] = {
    "GeoCon", "Robo-Miner", "Tiger", "Cargo Truck", "Panther", "Lynx", "ConVec",
    "Earthworker", "Repair Vehicle", "Robo-Dozer", "Evacuation Transport", "Robo-Surveyor",
    "Scout", "Scorpion", "Scorpion 3-Pack", "Spider", "Spider 3-Pack",
};
static const char* s_vehName[0x11] = {};
const char* vehicleDisplayName(int type) {
    return (type >= 1 && type <= 0x10 && s_vehName[type]) ? s_vehName[type] : "";
}
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

struct VehDesc { VehPlayerStats player[7]; int32_t overflow[17] = {}; };
static VehDesc s_veh[0x11];
static int s_vehTrack[0x11] = {};
static int s_vehTopic[0x11] = {};
int vehicleResearchTopic(int type) { return (type >= 1 && type <= 0x10) ? s_vehTopic[type] : 0; }
int vehicleTrackClass(int type) { return (type >= 1 && type <= 0x10) ? s_vehTrack[type] : 0; }
static bool s_vehLoaded = false;

const VehPlayerStats* vehicleSheet(int type, int player) {
    if (type < 1 || type > 0x10 || player < 0 || player > 6) return nullptr;
    return &s_veh[type].player[player];
}

bool loadVehicleSheet() {
    if (s_vehLoaded) return true;
    std::vector<std::string> lines;
    if (!readSheetLines("vehicles.txt", lines)) return false;
    size_t row = 0;
    for (size_t li = 2; li < lines.size(); ++li) {
        std::vector<SheetCol> c = splitRow(lines[li]);
        if (c.size() < 18 || c[0].text.empty()) continue;
        const size_t thisRow = row++;
        int type = -1;
        for (int t = 1; t <= 0x10; ++t) {
            const char* n = vehicleCodeName(t);
            if (n && c[0].text == n) { type = t; break; }
        }
        if (type < 0) continue;   // ex. SCORPION_PACK: sem descritor próprio
        if (thisRow < sizeof(kVehicleRowNames) / sizeof(kVehicleRowNames[0]))
            s_vehName[type] = kVehicleRowNames[thisRow];
        switch (c[2].text.empty() ? 0 : c[2].text[0]) {            // desc+0x1E8
            case 'H': case 'h': s_vehTrack[type] = 4; break;
            case 'L': case 'l': s_vehTrack[type] = 0; break;
            case 'M': case 'm': s_vehTrack[type] = 2; break;
            case 'T': case 't': s_vehTrack[type] = 3; break;
            case 'W': case 'w': s_vehTrack[type] = 1; break;
            default: break;
        }
        s_vehTopic[type] = c[16].value;                            // desc+0x1E4
        VehPlayerStats& s = s_veh[type].player[0];
        s.hitPoints = c[4].value;        // +0x00
        s.armor = armorValue(c[5].value); // +0x08 (col 5 Armor Class → dword_4E1510)
        s.commonOre = c[8].value;        // +0x0C
        s.rareOre = c[9].value;          // +0x10
        s.cargo = c[13].value;           // +0x38
        s.repair = c[18].value;          // +0x04
        s.moveSpeed = c[6].value;        // +0x24 (desc+0x2C)
        s.turnRate = c[7].value;         // +0x28 (desc+0x30)
        s.buildTime = c[10].value;       // +0x14 (desc+0x1C)
        s.sight = c[14].value;           // +0x18 (desc+0x20)
        s.productionRate = c[17].value;  // +0x2C (desc+0x34)
    }
    for (int t = 1; t <= 0x10; ++t)
        for (int pl = 1; pl < 7; ++pl) s_veh[t].player[pl] = s_veh[t].player[0];
    s_vehLoaded = true;
    return true;
}

bool loadBuildingSheet() {
    if (s_loaded) return true;
    std::vector<std::string> lines;
    if (!readSheetLines("building.txt", lines)) return false;
    // 2 linhas de cabeçalho; a 3ª leitura (sub_411220) já é a 1ª linha de dados.
    size_t row = 0;
    for (size_t li = 2; li < lines.size(); ++li) {
        std::vector<SheetCol> c = splitRow(lines[li]);
        if (c.size() < 32 || c[0].text.empty()) continue;
        const size_t thisRow = row++;
        // Coluna 0: Code_Name → descritor pelo GetName (loc_445127).
        int type = -1;
        for (int t = 0x15; t <= 0x3A; ++t)
            if (c[0].text == structureCodeName(t)) { type = t; break; }
        if (type < 0) continue;                      // loc_44563F: linha ignorada
        StructSheetDesc& d = s_desc[type];
        StructPlayerStats& s = d.player[0];
        if (thisRow < sizeof(kBuildingRowNames) / sizeof(kBuildingRowNames[0]))
            d.displayName = kBuildingRowNames[thisRow];
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
        s.armor = armorValue(c[7].value);   // +0x08 (dword_4E1510[classe])
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

namespace op2 {

// Nomes de exibição das armas: off_4E9590 avança 1 por linha CASADA do
// WEAPONS.TXT (todas casam com um descritor) — tabela lida do OUTPOST2.EXE.
static const char* const kWeaponRowNames[] = {
    "Laser", "Microwave", "Stickyfoam", "Rail Gun", "EMP", "RPG", "Starflare", "Starflare",
    "ESG", "Supernova", "Supernova", "Acid Cloud", "Thor's Hammer", "", "Damage of EMP Missile",
    "Inter-colony Shuttle", "Magma Vents", "Fumaroles", "Mine Beacons", "Pretty art!",
    "Earthquake", "Lava Eruption", "Electrical Storm", "Tornado", "Meteor",
    "Normal Vehicle Self-destruct", "Disasterous Building Explosion",
    "Catastrophic Building Explosion", "Athiest Building Explosion",
};
static WeaponSheetDesc s_weap[0x73];
static bool s_weapLoaded = false;

const WeaponSheetDesc* weaponSheet(int type) {
    if (type < 0 || type >= 0x73 || !s_weap[type].loaded) return nullptr;
    return &s_weap[type];
}

bool loadWeaponSheet() {
    if (s_weapLoaded) return true;
    std::vector<std::string> lines;
    if (!readSheetLines("weapons.txt", lines)) return false;
    size_t row = 0;
    for (size_t li = 2; li < lines.size(); ++li) {
        std::vector<SheetCol> c = splitRow(lines[li]);
        if (c.size() < 14 || c[0].text.empty()) continue;
        const int type = unitTypeFromCodeName(c[0].text.c_str());   // GetName de 1..0x72
        if (type < 0 || type >= 0x73) continue;                    // loc_445CA1
        WeaponSheetDesc& d = s_weap[type];
        if (row < sizeof(kWeaponRowNames) / sizeof(kWeaponRowNames[0])) d.displayName = kWeaponRowNames[row];
        ++row;
        WeapPlayerStats& s = d.player[0];
        d.radius = c[2].value;                                     // +0x248
        d.skipped = c[3].value;                                    // +0x24C
        s.range = c[4].value;                                      // +0x20 (bloco+0x18)
        int tr = c[5].value;                                       // clamp 1..16
        if (tr <= 1) tr = 1;
        if (tr >= 0x10) tr = 0x10;
        s.turnRate = tr;                                           // +0x2C (bloco+0x24)
        switch (c[6].text.empty() ? 0 : c[6].text[0]) {           // +0x1F4
            case 'B': case 'b': d.ownerFlags |= 0x800 | 0x1000; break;
            case 'E': case 'e': d.ownerFlags |= 0x1000; break;
            case 'P': case 'p': d.ownerFlags |= 0x800; break;
            default: break;
        }
        s.concussion = c[7].value;                                 // +0x30 (bloco+0x28)
        s.penetration = c[8].value;                                // +0x34 (bloco+0x2C)
        s.commonOre = c[9].value;                                  // +0x14
        s.rareOre = c[10].value;                                   // +0x18
        s.buildTime = c[11].value;                                 // +0x1C
        s.reload = c[12].value;                                    // +0x38 (bloco+0x30)
        d.researchTopic = c[13].value;                             // +0x1E4
        d.loaded = true;
    }
    for (auto& d : s_weap)
        for (int pl = 1; pl < 7; ++pl) d.player[pl] = d.player[0];
    s_weapLoaded = true;
    return true;
}

// Blocos dos demais tipos (paredes, espaçonaves, desastres...): só melhorias.
struct OtherDesc { int32_t block[7][17] = {}; int32_t overflow[17] = {}; };
static OtherDesc s_other[0x73];

uint8_t* unitStatBlock(int type, int player) {
    if (player < 0 || player > 6 || type < 1 || type > 0x72) return nullptr;
    if (type >= 0x15 && type <= 0x3A && s_desc[type].loaded)
        return reinterpret_cast<uint8_t*>(&s_desc[type].player[player]);
    if (type >= 1 && type <= 0x10) return reinterpret_cast<uint8_t*>(&s_veh[type].player[player]);
    if (s_weap[type].loaded) return reinterpret_cast<uint8_t*>(&s_weap[type].player[player]);
    return reinterpret_cast<uint8_t*>(&s_other[type].block[player][0]);
}

void reloadSheets() {
    for (auto& d : s_desc) d = StructSheetDesc{};
    for (auto& v : s_veh) v = VehDesc{};
    for (auto& w : s_weap) w = WeaponSheetDesc{};
    for (auto& o : s_other) o = OtherDesc{};
    for (int& t : s_vehTrack) t = 0;
    for (int& t : s_vehTopic) t = 0;
    s_loaded = s_vehLoaded = s_weapLoaded = false;
    loadBuildingSheet();
    loadVehicleSheet();
    loadWeaponSheet();
}

// WEAPONS.TXT (parte de armas de sub_445070) — por ora só a coluna 13
// "Research Topic" → desc+0x1E4, usada pela moral (técnicas de aviso de
// desastre: LIGHTNING/TORNADO/EARTHQUAKE/ERUPTION/METEOR).
int weaponResearchTopic(const char* codeName) {
    static std::vector<std::pair<std::string, int>> s_topics;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::vector<std::string> lines;
        if (readSheetLines("weapons.txt", lines))
            for (size_t li = 2; li < lines.size(); ++li) {
                std::vector<SheetCol> c = splitRow(lines[li]);
                if (c.size() >= 14 && !c[0].text.empty())
                    s_topics.emplace_back(c[0].text, c[13].value);
            }
    }
    for (const auto& t : s_topics) if (t.first == codeName) return t.second;
    return 0;
}

} // namespace op2
