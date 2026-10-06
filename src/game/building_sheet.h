#pragma once

#include <cstdint>

namespace op2 {

// Dados das ESTRUTURAS lidos do BUILDING.TXT (SHEETS.VOL) — porta a parte de
// prédios de sub_445070 @113721, que preenche o descritor off_4E1340[map_id]
// casando a coluna 0 (Code_Name) com o GetName (vt[1]) de cada descritor.
//
// Bloco por jogador (0x44 bytes em desc+8+jogador*0x44; offsets da tabela de
// campos @4D5C40): +0x00 Hit_Points, +0x08 Armor, +0x0C Common_Required,
// +0x10 Rare_Required, +0x14 Build_Time (coluna "Build Points"), +0x18 Sight,
// +0x24 Power_Required, +0x28 Workers, +0x2C Scientists, +0x30 Storage_Capacity,
// +0x34 Production_Capacity, +0x38 Storage_Bays. Após ler todas as planilhas o
// bloco 0 é copiado p/ os blocos 1..6 (loc_445EDA); upgrades mudam cada um depois.
struct StructPlayerStats {
    int hitPoints = 0;        // +0x00
    int armor = 0;            // +0x08 (dword_4E1510[classe])
    int commonOre = 0;        // +0x0C
    int rareOre = 0;          // +0x10
    int buildTime = 0;        // +0x14 ("Build Points")
    int sight = 0;            // +0x18
    int power = 0;            // +0x24
    int workers = 0;          // +0x28
    int scientists = 0;       // +0x2C
    int storageCapacity = 0;  // +0x30
    int productionCapacity = 0; // +0x34
    int storageBays = 0;      // +0x38
};

struct StructSheetDesc {
    bool loaded = false;
    int  sizeX = 1, sizeY = 1;   // +0x248 / +0x24A
    uint32_t ownerFlags = 0;     // +0x1F4: 0x1000 = Eden, 0x800 = Plymouth ("B" = ambos)
    uint32_t flags24C = 0;       // +0x24C: bit0 Border, bit1 Kit, bits2-5 Docking,
                                 //         bit6 AutoTargeted, bits7-8 Explosion Size
    uint8_t  rareRubble = 0;     // +0x2A1 ("Rare Rubble")
    uint8_t  commonRubble = 0;   // +0x2A3 ("Normal Rubble")
    uint8_t  dockLocE = 0, dockLocP = 0;  // +0x2A4 / +0x2A5
    uint8_t  resourcePriority = 0;        // +0x2A0
    int      researchTopic = 0;           // +0x1E4
    StructPlayerStats player[7];          // desc+8 + p*0x44
    int explosionSize() const { return static_cast<int>((flags24C >> 7) & 3); }
};

// Carrega o BUILDING.TXT (idempotente). Retorna false se a planilha não abriu.
bool loadBuildingSheet();
// Descritor da estrutura (map_id 0x15..0x3A) ou nullptr.
const StructSheetDesc* structSheet(int type);

// Parte de VEÍCULOS de sub_445070 (VEHICLES.TXT), só as colunas usadas até
// aqui: col 4 Hit Points → bloco+0x00, col 10 Build Time → bloco+0x14,
// col 17 Production Rate → bloco+0x2C (desc+0x34; multiplicador /128 da
// duração de construção em sub_43DE50).
struct VehPlayerStats {
    int hitPoints = 0;        // +0x00
    int buildTime = 0;        // +0x14
    int productionRate = 0;   // +0x2C
};
bool loadVehicleSheet();
// Bloco do jogador `player` do veículo `type` (0x01..0x10) ou nullptr.
const VehPlayerStats* vehicleSheet(int type, int player);

} // namespace op2
