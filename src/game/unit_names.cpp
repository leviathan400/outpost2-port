#include "unit_names.h"

#include <cstring>

#include "structure_anim.h"
#include "vehicle_anim.h"

namespace op2 {

// Tipos fora das tabelas de veículo (1..0x10) e prédio (0x15..0x3A).
static const struct { int type; const char* name; } kOther[] = {
    {0x11, "TUBE"}, {0x12, "WALL"}, {0x13, "WALL_LAVA"}, {0x14, "WALL_MICROBE"},
    {0x3B, "ACID"}, {0x3C, "EMP"}, {0x3D, "LASER"}, {0x3E, "MICROWAVE"},
    {0x3F, "RAIL_GUN"}, {0x40, "CANNON"}, {0x41, "SELF_DESTRUCT"}, {0x42, "SELF_DESTRUCT_ADV"},
    {0x43, "TURRET_DESTRUCT"}, {0x44, "TURRET_DESTRUCT_ADV"}, {0x45, "SCUTTLE_DESTRUCT"},
    {0x46, "SPAM"}, {0x47, "FOAM"}, {0x48, "THORS_HAMMER"}, {0x49, "SCORPION_WEAPON"},
    {0x4A, "EMP_BIG"}, {0x4B, "BFG"}, {0x4C, "LIGHTNING"}, {0x4D, "TORNADO"},
    {0x4E, "EARTHQUAKE"}, {0x4F, "ERUPTION"}, {0x50, "METEOR"}, {0x51, "BEACON"},
    {0x52, "MAGMA_VENT"}, {0x53, "FUMAROLE"}, {0x54, "WRECK_FLAG"}, {0x55, "BLD_EXPL_SML"},
    {0x56, "BLD_EXPL_MED"}, {0x57, "BLD_EXPL_BIG"}, {0x58, "EDWARD"}, {0x59, "SOLAR_SAT"},
    {0x5A, "SPACE_1"}, {0x5B, "SPACE_2"}, {0x5C, "SPACE_3"}, {0x5D, "SPACE_4"},
    {0x5E, "SPACE_5"}, {0x5F, "SPACE_6"}, {0x60, "SPACE_7"}, {0x61, "SPACE_8"},
    {0x62, "SPACE_9"}, {0x63, "SPACE_10"}, {0x64, "SPACE_11"}, {0x65, "SPACE_12"},
    {0x66, "SPACE_13"}, {0x67, "SPACE_14"}, {0x68, "SPACE_15"}, {0x69, "SULV"},
    {0x6A, "RLV"}, {0x6B, "EMP_MISSILE"}, {0x6C, "LUX_1"}, {0x6D, "LUX_2"},
    {0x6E, "LUX_3"}, {0x6F, "SHUTTLE"}, {0x70, "SPIDER_PACK"}, {0x71, "SCORPION_PACK"},
    {0x72, "AMBIENT_ANIM"},
};

const char* unitCodeName(int type) {
    if (type >= 1 && type <= 0x10) return vehicleCodeName(type);
    if (type >= 0x15 && type <= 0x3A) return structureCodeName(type);
    for (const auto& o : kOther) if (o.type == type) return o.name;
    return nullptr;
}

int unitTypeFromCodeName(const char* name) {
    if (!name) return -1;
    for (int t = 1; t <= 0x72; ++t) {
        const char* n = unitCodeName(t);
        if (n && std::strcmp(n, name) == 0) return t;
    }
    return -1;
}

} // namespace op2
