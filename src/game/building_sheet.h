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
    const char* displayName = "";         // +0x1F8 (tabela fixa @0x4E9440 pela LINHA)
    StructPlayerStats player[7];          // desc+8 + p*0x44
    int explosionSize() const { return static_cast<int>((flags24C >> 7) & 3); }
};

// Carrega o BUILDING.TXT (idempotente). Retorna false se a planilha não abriu.
bool loadBuildingSheet();
// dword_4E1510: classe de blindagem (coluna Armor) → valor que o engine guarda no
// bloco do descritor (+0x08) e usa no dano: (valor * Concussion >> 8) + Penetration.
inline int armorValue(int armorClass) {
    static const int k[6] = {256, 180, 130, 90, 60, 40};
    return (armorClass >= 0 && armorClass < 6) ? k[armorClass] : 256;
}

// Descritor da estrutura (map_id 0x15..0x3A) ou nullptr.
const StructSheetDesc* structSheet(int type);

// Parte de VEÍCULOS de sub_445070 (VEHICLES.TXT), só as colunas usadas até
// aqui: col 4 Hit Points → bloco+0x00, col 6 Move Points → +0x24 (Move_Speed),
// col 7 Turn Rate → +0x28, col 10 Build Time → +0x14, col 17 Production Rate
// → +0x2C (desc+0x34; multiplicador /128 da duração de construção).
struct VehPlayerStats {
    int hitPoints = 0;        // +0x00
    int armor = 0;            // +0x08 (classe; o engine guarda dword_4E1510[classe])
    int buildTime = 0;        // +0x14
    int sight = 0;            // +0x18 col 14 Sight range (tiles)
    int moveSpeed = 0;        // +0x24 (sub_44B650: duração do passo)
    int turnRate = 0;         // +0x28 (sub_44B650: giro por tick)
    int productionRate = 0;   // +0x2C
};
// Classe de movimento do veículo (desc+0x1E8): coluna 2 "Track Type" do
// VEHICLES.TXT — L=0, W=1, M=2, T=3, H=4. 0 p/ tipos sem linha.
int vehicleTrackClass(int type);

// Nome de exibição do veículo (+0x1F8; tabela fixa @0x4E93FC pela linha do VEHICLES.TXT).
const char* vehicleDisplayName(int type);
bool loadVehicleSheet();
// Bloco do jogador `player` do veículo `type` (0x01..0x10) ou nullptr.
const VehPlayerStats* vehicleSheet(int type, int player);

// Parte de ARMAS de sub_445070 (WEAPONS.TXT). Colunas → descritor: 2 Damage
// Radius → +0x248, 3 Pixels Skipped → +0x24C, 4 Weapon Range → bloco+0x18,
// 5 Turret Turn Rate (1..16) → +0x24, 6 Owner → +0x1F4, 7 Concussion → +0x28,
// 8 Penetration → +0x2C, 9/10 Ore → +0x0C/+0x10, 11 Build Time → +0x14,
// 12 Reload → +0x30, 13 Research → desc+0x1E4. Bloco 0 copiado p/ 1..6.
struct WeapPlayerStats {
    int range = 0, turnRate = 0, concussion = 0, penetration = 0, reload = 0;
    int commonOre = 0, rareOre = 0, buildTime = 0;
};
struct WeaponSheetDesc {
    bool loaded = false;
    int radius = 0;               // +0x248 (pixels)
    int skipped = 0;              // +0x24C pixels pulados ao disparar (alcance mínimo)
    uint32_t ownerFlags = 0;      // +0x1F4
    int researchTopic = 0;        // +0x1E4
    const char* displayName = ""; // +0x1F8 (tabela fixa @off_4E9590 pela linha casada)
    WeapPlayerStats player[7];
};
bool loadWeaponSheet();
const WeaponSheetDesc* weaponSheet(int type);

// WEAPONS.TXT col 13 (Research Topic, desc+0x1E4) pelo Code_Name; 0 se não houver.
int weaponResearchTopic(const char* codeName);

} // namespace op2
