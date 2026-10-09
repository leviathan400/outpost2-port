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
// O bloco tem EXATAMENTE o layout do original (17 dwords): as melhorias de
// pesquisa (UNIT_PROP, sub_473860) escrevem por offset — ver unitStatBlock().
struct StructPlayerStats {
    int32_t hitPoints = 0;        // +0x00
    int32_t repair = 0;           // +0x04
    int32_t armor = 0;            // +0x08 valor (dword_4E1510[classe])
    int32_t commonOre = 0;        // +0x0C
    int32_t rareOre = 0;          // +0x10
    int32_t buildTime = 0;        // +0x14 ("Build Points")
    int32_t sight = 0;            // +0x18
    int32_t f1C = 0, f20 = 0;     // +0x1C/+0x20 ("Improved" grava em +0x1D)
    int32_t power = 0;            // +0x24
    int32_t workers = 0;          // +0x28
    int32_t scientists = 0;       // +0x2C
    int32_t storageCapacity = 0;  // +0x30
    int32_t productionCapacity = 0; // +0x34
    int32_t storageBays = 0;      // +0x38
    int32_t improvedBy[2] = {};   // +0x3C/+0x40 tecnologias que melhoraram o tipo
};
static_assert(sizeof(StructPlayerStats) == 0x44, "bloco de prédio: 0x44");

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
    const char* produceName = "";         // +0x220 nome na lista de produção (@0x4E94E8 pela LINHA)
    StructPlayerStats player[7];          // desc+8 + p*0x44
    int32_t overflow[17] = {};            // além do 7º bloco (gravações de melhoria)
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
// TUBE/WALL/WALL_LAVA/WALL_MICROBE (0x11..0x14): linhas do building.txt (custo,
// facção +0x1F4, Research Topic +0x1E4) — descritores próprios no original.
const StructSheetDesc* wallSheet(int type);

// Parte de VEÍCULOS de sub_445070 (VEHICLES.TXT), só as colunas usadas até
// aqui: col 4 Hit Points → bloco+0x00, col 6 Move Points → +0x24 (Move_Speed),
// col 7 Turn Rate → +0x28, col 10 Build Time → +0x14, col 17 Production Rate
// → +0x2C (desc+0x34; multiplicador /128 da duração de construção).
struct VehPlayerStats {
    int32_t hitPoints = 0;        // +0x00 col 4
    int32_t repair = 0;           // +0x04 col 18 Repair Amt
    int32_t armor = 0;            // +0x08 valor (dword_4E1510[col 5])
    int32_t commonOre = 0;        // +0x0C col 8
    int32_t rareOre = 0;          // +0x10 col 9
    int32_t buildTime = 0;        // +0x14 col 10
    int32_t sight = 0;            // +0x18 col 14 Sight range (tiles)
    int32_t f1C = 0, f20 = 0;
    int32_t moveSpeed = 0;        // +0x24 col 6 (sub_44B650: duração do passo)
    int32_t turnRate = 0;         // +0x28 col 7 (sub_44B650: giro por tick)
    int32_t productionRate = 0;   // +0x2C col 17
    int32_t f30 = 0, f34 = 0;
    int32_t cargo = 0;            // +0x38 col 13 Cargo Capacity
    int32_t improvedBy[2] = {};   // +0x3C/+0x40
};
static_assert(sizeof(VehPlayerStats) == 0x44, "bloco de veiculo: 0x44");
// Classe de movimento do veículo (desc+0x1E8): coluna 2 "Track Type" do
// VEHICLES.TXT — L=0, W=1, M=2, T=3, H=4. 0 p/ tipos sem linha.
int vehicleTrackClass(int type);
// Research Topic do veículo (col 16 → desc+0x1E4, ID da tecnologia); 0 se não há.
int vehicleResearchTopic(int type);

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
    int32_t f00 = 0, f04 = 0, f08 = 0;
    int32_t commonOre = 0;        // +0x0C col 9
    int32_t rareOre = 0;          // +0x10 col 10
    int32_t buildTime = 0;        // +0x14 col 11
    int32_t range = 0;            // +0x18 col 4 (tiles)
    int32_t f1C = 0, f20 = 0;
    int32_t turnRate = 0;         // +0x24 col 5 (1..16)
    int32_t concussion = 0;       // +0x28 col 7
    int32_t penetration = 0;      // +0x2C col 8
    int32_t reload = 0;           // +0x30 col 12
    int32_t f34 = 0, f38 = 0;
    int32_t improvedBy[2] = {};   // +0x3C/+0x40
};
static_assert(sizeof(WeapPlayerStats) == 0x44, "bloco de arma: 0x44");
struct WeaponSheetDesc {
    bool loaded = false;
    int radius = 0;               // +0x248 (pixels)
    int skipped = 0;              // +0x24C pixels pulados ao disparar (alcance mínimo)
    uint32_t ownerFlags = 0;      // +0x1F4
    int researchTopic = 0;        // +0x1E4
    const char* displayName = ""; // +0x1F8 (tabela fixa @off_4E9590 pela linha casada)
    WeapPlayerStats player[7];
    int32_t overflow[17] = {};
};
bool loadWeaponSheet();
const WeaponSheetDesc* weaponSheet(int type);

// Bloco 0x44 do jogador p/ QUALQUER tipo 1..0x72 (desc+8+p*0x44), como bytes —
// as melhorias de pesquisa gravam dwords por offset (até desalinhados) e podem
// passar do bloco (como no original). nullptr fora da faixa.
uint8_t* unitStatBlock(int type, int player);
// Relê as planilhas (início de cada partida, sub_4893A0 → sub_445070): desfaz
// as melhorias da partida anterior.
void reloadSheets();

// WEAPONS.TXT col 13 (Research Topic, desc+0x1E4) pelo Code_Name; 0 se não houver.
int weaponResearchTopic(const char* codeName);

// SPACE.TXT (parte espacial de sub_445070): itens 0x58..0x6E (satélites,
// módulos da nave, cargas, SULV/RLV/míssil EMP, bens de consumo). Colunas:
// 2 Owner → +0x1F4, 3 Common → bloco+0x0C, 4 Rare → +0x10, 5 Build Time →
// +0x14, 6 Research Topic → +0x1E4; nome +0x1F8 da tabela fixa @off_4E9604.
bool loadSpaceSheet();

// Campos do descritor off_4E1340[type] p/ QUALQUER tipo carregado das
// planilhas (prédios, paredes, veículos — incl. os pacotes 0x70/0x71 —,
// armas e itens espaciais):
uint32_t unitOwnerFlags(int type);        // +0x1F4 (0x800 Plymouth, 0x1000 Eden)
const char* unitDisplayName(int type);    // +0x1F8 ("" se não há)
const char* unitProduceName(int type);    // +0x220 (só prédios/paredes; "" nos demais)
int unitResearchTopic(int type);          // +0x1E4 (ID da tecnologia; 0 = nenhuma)

} // namespace op2
