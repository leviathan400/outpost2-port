#include "vehicle_anim.h"

namespace op2 {

// Tabela de animação por veículo (map_id → 16 direções), tabela A do descritor
// (off_4E1340[type] + 0x24C). Extraída do binário (inits sub_447xxx/etc.) e mapeada
// por map_id pelo GetName (vt[1]) de cada descritor. NÃO EDITAR à mão.
// map_id 15 (SCORPION_PACK) e 17 (SPIDER_PACK) = variantes implantadas (sem tabela
// direcional própria aqui); 0 = sem animação.
static const int kVehicleAnim[18][16] = {
    /*  0 none       */ {0},
    /*  1 GEO_METRO  */ { 1255, 1270, 1269, 1268, 1267, 1266, 1265, 1264, 1263, 1262, 1261, 1260, 1259, 1258, 1257, 1256 },
    /*  2 MINER      */ { 1271, 1286, 1285, 1284, 1283, 1282, 1281, 1280, 1279, 1278, 1277, 1276, 1275, 1274, 1273, 1272 },
    /*  3 BIG_TANK   */ {  332,  332,  339,  339,  338,  338,  337,  337,  336,  336,  335,  335,  334,  334,  333,  333 },
    /*  4 BIG_TRUCK  */ {  109,  124,  123,  122,  121,  120,  119,  118,  117,  116,  115,  114,  113,  112,  111,  110 },
    /*  5 MED_TANK   */ {  340,  340,  347,  347,  346,  346,  345,  345,  344,  344,  343,  343,  342,  342,  341,  341 },
    /*  6 LIGHT_TANK */ { 1303, 1318, 1317, 1316, 1315, 1314, 1313, 1312, 1311, 1310, 1309, 1308, 1307, 1306, 1305, 1304 },
    /*  7 CON_TRUCK  */ {  388,  403,  402,  401,  400,  399,  398,  397,  396,  395,  394,  393,  392,  391,  390,  389 },
    /*  8 WALL_TRUCK */ { 1912, 1254, 1917, 1253, 1916, 1252, 1918, 1251, 1919, 1250, 1915, 1249, 1913, 1248, 1914, 1247 },
    /*  9 REPAIR_TRK */ { 1231, 1246, 1245, 1244, 1243, 1242, 1241, 1240, 1239, 1238, 1237, 1236, 1235, 1234, 1233, 1232 },
    /* 10 BULL_DOZER */ {  101,  101,  108,  108,  107,  107,  106,  106,  105,  105,  104,  104,  103,  103,  102,  102 },
    /* 11 MOBILE_HOME*/ {  404,  419,  418,  417,  416,  415,  414,  413,  412,  411,  410,  409,  408,  407,  406,  405 },
    /* 12 SURVEYOR   */ { 1227,  348, 1226,  355, 1225,  354, 1224,  353, 1223,  352, 1230,  351, 1229,  350, 1228,  349 },
    /* 13 SCOUT      */ {  543,  558,  557,  556,  555,  554,  553,  552,  551,  550,  549,  548,  547,  546,  545,  544 },
    /* 14 SCORPION   */ { 1750, 1765, 1764, 1763, 1762, 1761, 1760, 1759, 1758, 1757, 1756, 1755, 1754, 1753, 1752, 1751 },
    /* 15 SCORPION_P */ {0},
    /* 16 SPIDER     */ { 1734, 1749, 1748, 1747, 1746, 1745, 1744, 1743, 1742, 1741, 1740, 1739, 1738, 1737, 1736, 1735 },
    /* 17 SPIDER_PACK*/ {0},
};

bool hasVehicleAnim(int type) {
    return type >= 1 && type <= 17 && kVehicleAnim[type][0] != 0;
}

int vehicleAnimIndex(int type, int rotation) {
    if (!hasVehicleAnim(type)) return -1;
    int dir = ((rotation + 8) & 0xF0) >> 4;   // 16 direções (sub_44C5C0/sub_410E50)
    if (dir < 0 || dir > 15) dir = 0;
    int a = kVehicleAnim[type][dir];
    return a ? a : -1;
}

} // namespace op2
