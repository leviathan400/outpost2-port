#include "vehicle_anim.h"

namespace op2 {

// Tabelas de animacao dos VEICULOS por map_id REAL (indice de off_4E1340, = enum
// do SDK), GERADAS por tools/extract_anim_tables.py --emit. NAO EDITAR a mao.
// Fonte: init do descritor (vt[4] da vtable cujo vt[1] e GetName). QUATRO
// conjuntos de 16 direcoes por faccao (sub_44C4F0):
//   A (Eden, [unit+0x44] & 0x1000): +0x24C + conjunto*0x40
//   B (Plymouth):                   +0x34C + conjunto*0x40
//   conjunto 0 parado, 1 andando, 2 trabalhando (acao = desc+0x44C), 3 acao =
//   desc+0x450. Entrada 0 = cai na do conjunto 0.
// Valores CRUS como no descritor (indiceReal + 0x65); 0 = sem entrada.
static const int kVehicleAnim[0x11][4][16] = {
    /* 0x00 none         -          */ {
        {0},
        {0},
        {0},
        {0},
    },
    /* 0x01 BIG_TRUCK    sub_405D60 */ {
        { 109,  124,  123,  122,  121,  120,  119,  118,  117,  116,  115,  114,  113,  112,  111,  110 },
        {0},
        {0},
        {0},
    },
    /* 0x02 CON_TRUCK    sub_410B90 */ {
        { 388,  403,  402,  401,  400,  399,  398,  397,  396,  395,  394,  393,  392,  391,  390,  389 },
        {0},
        {   0,    0,    0,    0,    0,    0, 1796,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
        {1820,    0, 1827,    0, 1826,    0, 1825,    0, 1824,    0, 1823,    0, 1822,    0, 1821,    0 },
    },
    /* 0x03 SPIDER       sub_4475F0 */ {
        {1734, 1749, 1748, 1747, 1746, 1745, 1744, 1743, 1742, 1741, 1740, 1739, 1738, 1737, 1736, 1735 },
        {0},
        {0},
        {0},
    },
    /* 0x04 SCORPION     sub_447810 */ {
        {1750, 1765, 1764, 1763, 1762, 1761, 1760, 1759, 1758, 1757, 1756, 1755, 1754, 1753, 1752, 1751 },
        {0},
        {0},
        {0},
    },
    /* 0x05 LIGHT_TANK   sub_485670 */ {
        {1303, 1318, 1317, 1316, 1315, 1314, 1313, 1312, 1311, 1310, 1309, 1308, 1307, 1306, 1305, 1304 },
        {0},
        {0},
        {0},
    },
    /* 0x06 MED_TANK     sub_485850 */ {
        { 340,  340,  347,  347,  346,  346,  345,  345,  344,  344,  343,  343,  342,  342,  341,  341 },
        {0},
        {0},
        {0},
    },
    /* 0x07 BIG_TANK     sub_485A00 */ {
        { 332,  332,  339,  339,  338,  338,  337,  337,  336,  336,  335,  335,  334,  334,  333,  333 },
        {0},
        {0},
        {0},
    },
    /* 0x08 SURVEYOR     sub_484210 */ {
        {1227,  348, 1226,  355, 1225,  354, 1224,  353, 1223,  352, 1230,  351, 1229,  350, 1228,  349 },
        { 348,    0,  355,    0,  354,    0,  353,    0,  352,    0,  351,    0,  350,    0,  349,    0 },
        {0},
        {0},
    },
    /* 0x09 MINER        sub_4479D0 */ {
        {1271, 1286, 1285, 1284, 1283, 1282, 1281, 1280, 1279, 1278, 1277, 1276, 1275, 1274, 1273, 1272 },
        {0},
        {0},
        {0},
    },
    /* 0x0A GEO_METRO    sub_447B60 */ {
        {1255, 1270, 1269, 1268, 1267, 1266, 1265, 1264, 1263, 1262, 1261, 1260, 1259, 1258, 1257, 1256 },
        {0},
        {0},
        {0},
    },
    /* 0x0B SCOUT        sub_474890 */ {
        { 543,  558,  557,  556,  555,  554,  553,  552,  551,  550,  549,  548,  547,  546,  545,  544 },
        {0},
        {0},
        {0},
    },
    /* 0x0C BULL_DOZER   sub_409AB0 */ {
        { 101,  101,  108,  108,  107,  107,  106,  106,  105,  105,  104,  104,  103,  103,  102,  102 },
        {0},
        {1766, 1766, 1773, 1773, 1772, 1772, 1771, 1771, 1770, 1770, 1769, 1769, 1768, 1768, 1767, 1767 },
        {0},
    },
    /* 0x0D MOBILE_HOME  sub_448970 */ {
        { 404,  419,  418,  417,  416,  415,  414,  413,  412,  411,  410,  409,  408,  407,  406,  405 },
        {0},
        {0},
        {0},
    },
    /* 0x0E REPAIR_TRUCK sub_448770 */ {
        {1231, 1246, 1245, 1244, 1243, 1242, 1241, 1240, 1239, 1238, 1237, 1236, 1235, 1234, 1233, 1232 },
        {0},
        {1726, 1726, 1733, 1733, 1732, 1732, 1731, 1731, 1730, 1730, 1729, 1729, 1728, 1728, 1727, 1727 },
        {0},
    },
    /* 0x0F WALL_TRUCK   sub_448540 */ {
        {1912, 1254, 1917, 1253, 1916, 1252, 1918, 1251, 1919, 1250, 1915, 1249, 1913, 1248, 1914, 1247 },
        {0},
        {1804,    0, 1811,    0, 1810,    0, 1809,    0, 1808,    0, 1807,    0, 1806,    0, 1805,    0 },
        {0},
    },
    /* 0x10 SCAT         sub_4746B0 */ {
        { 558,  557,  556,  555,  554,  553,  552,  551,  550,  549,  548,  547,  546,  545,  544,    0 },
        {0},
        {0},
        {   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,  561 },
    },
};

// Tabela B (+0x34C).
static const int kVehicleAnimB[0x11][4][16] = {
    /* 0x00 none         -          */ {
        {0},
        {0},
        {0},
        {0},
    },
    /* 0x01 BIG_TRUCK    sub_405D60 */ {
        { 125,  140,  139,  138,  137,  136,  135,  134,  133,  132,  131,  130,  129,  128,  127,  126 },
        {0},
        {0},
        {0},
    },
    /* 0x02 CON_TRUCK    sub_410B90 */ {
        { 420,  435,  434,  433,  432,  431,  430,  429,  428,  427,  426,  425,  424,  423,  422,  421 },
        {0},
        {   0,    0,    0,    0,    0,    0, 1797,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
        {1828,    0, 1835,    0, 1834,    0, 1833,    0, 1832,    0, 1831,    0, 1830,    0, 1829,    0 },
    },
    /* 0x03 SPIDER       sub_4475F0 */ {
        {1734, 1749, 1748, 1747, 1746, 1745, 1744, 1743, 1742, 1741, 1740, 1739, 1738, 1737, 1736, 1735 },
        {0},
        {1782, 1782, 1789, 1789, 1788, 1788, 1787, 1787, 1786, 1786, 1785, 1785, 1784, 1784, 1783, 1783 },
        {0},
    },
    /* 0x04 SCORPION     sub_447810 */ {
        {1750, 1765, 1764, 1763, 1762, 1761, 1760, 1759, 1758, 1757, 1756, 1755, 1754, 1753, 1752, 1751 },
        {0},
        {0},
        {0},
    },
    /* 0x05 LIGHT_TANK   sub_485670 */ {
        {1328, 1343, 1342, 1341, 1340, 1339, 1338, 1337, 1336, 1335, 1334, 1333, 1332, 1331, 1330, 1329 },
        {0},
        {0},
        {0},
    },
    /* 0x06 MED_TANK     sub_485850 */ {
        { 436,  436,  443,  443,  442,  442,  441,  441,  440,  440,  439,  439,  438,  438,  437,  437 },
        {0},
        {0},
        {0},
    },
    /* 0x07 BIG_TANK     sub_485A00 */ {
        { 380,  380,  387,  387,  386,  386,  385,  385,  384,  384,  383,  383,  382,  382,  381,  381 },
        {0},
        {0},
        {0},
    },
    /* 0x08 SURVEYOR     sub_484210 */ {
        {2126,  356, 2133,  363, 2132,  362, 2131,  361, 2130,  360, 2129,  359, 2128,  358, 2127,  357 },
        { 356,    0,  363,    0,  362,    0,  361,    0,  360,    0,  359,    0,  358,    0,  357,    0 },
        {0},
        {0},
    },
    /* 0x09 MINER        sub_4479D0 */ {
        {1287, 1302, 1301, 1300, 1299, 1298, 1297, 1296, 1295, 1294, 1293, 1292, 1291, 1290, 1289, 1288 },
        {0},
        {0},
        {0},
    },
    /* 0x0A GEO_METRO    sub_447B60 */ {
        {1255, 1255, 1269, 1269, 1267, 1267, 1265, 1265, 1263, 1263, 1261, 1261, 1259, 1259, 1257, 1257 },
        {0},
        {0},
        {0},
    },
    /* 0x0B SCOUT        sub_474890 */ {
        { 561,  576,  575,  574,  573,  572,  571,  570,  569,  568,  567,  566,  565,  564,  563,  562 },
        {0},
        {0},
        {0},
    },
    /* 0x0C BULL_DOZER   sub_409AB0 */ {
        { 444,  444,  451,  451,  450,  450,  449,  449,  448,  448,  447,  447,  446,  446,  445,  445 },
        {0},
        {1774, 1774, 1781, 1781, 1780, 1780, 1779, 1779, 1778, 1778, 1777, 1777, 1776, 1776, 1775, 1775 },
        {0},
    },
    /* 0x0D MOBILE_HOME  sub_448970 */ {
        { 364,  379,  378,  377,  376,  375,  374,  373,  372,  371,  370,  369,  368,  367,  366,  365 },
        {0},
        {0},
        {0},
    },
    /* 0x0E REPAIR_TRUCK sub_448770 */ {
        {1231, 1231, 1245, 1245, 1243, 1243, 1241, 1241, 1239, 1239, 1237, 1237, 1235, 1235, 1233, 1233 },
        {0},
        {0},
        {0},
    },
    /* 0x0F WALL_TRUCK   sub_448540 */ {
        {1920, 1935, 1925, 1934, 1924, 1933, 1927, 1932, 1926, 1931, 1923, 1930, 1921, 1929, 1922, 1928 },
        {0},
        {1812,    0, 1819,    0, 1818,    0, 1817,    0, 1816,    0, 1815,    0, 1814,    0, 1813,    0 },
        {0},
    },
    /* 0x10 SCAT         sub_4746B0 */ {
        { 576,  575,  574,  573,  572,  571,  570,  569,  568,  567,  566,  565,  564,  563,  562,    0 },
        {0},
        {0},
        {0},
    },
};

// As acoes que pedem os conjuntos 2 e 3: desc+0x44C / +0x450 (0x35 = nenhuma,
// o padrao do construtor-base sub_44B400).
static const int kVehicleWorkAction[0x11][2] = {
    { 0x35, 0x35 },  // 0x00 none
    { 0x35, 0x35 },  // 0x01 BIG_TRUCK
    { 0x06, 0x1C },  // 0x02 CON_TRUCK
    { 0x1C, 0x35 },  // 0x03 SPIDER
    { 0x35, 0x35 },  // 0x04 SCORPION
    { 0x35, 0x35 },  // 0x05 LIGHT_TANK
    { 0x35, 0x35 },  // 0x06 MED_TANK
    { 0x35, 0x35 },  // 0x07 BIG_TANK
    { 0x35, 0x35 },  // 0x08 SURVEYOR
    { 0x35, 0x35 },  // 0x09 MINER
    { 0x35, 0x35 },  // 0x0A GEO_METRO
    { 0x35, 0x35 },  // 0x0B SCOUT
    { 0x01, 0x35 },  // 0x0C BULL_DOZER
    { 0x35, 0x35 },  // 0x0D MOBILE_HOME
    { 0x1C, 0x35 },  // 0x0E REPAIR_TRUCK
    { 0x07, 0x35 },  // 0x0F WALL_TRUCK
    { 0x35, 0x35 },  // 0x10 SCAT
};

// Vies entre o valor guardado no descritor/[unit+0x40] e o indice real no array
// de animacao (sub_403E00 faz `lea eax, [ecx-65h]`).
static const int kAnimIndexBias = 0x65;

bool hasVehicleAnim(int type) {
    return type >= 1 && type <= 0x10 && kVehicleAnim[type][0][0] != 0;
}

int vehicleAnimIndexSet(int type, int rotation, bool isEden, int set) {
    if (!hasVehicleAnim(type)) return -1;
    int dir = ((rotation + 8) & 0xF0) >> 4;   // 16 direcoes (sub_44C5C0/sub_410E50)
    if (dir < 0 || dir > 15) dir = 0;
    if (set < 0 || set > 3) set = 0;
    // sub_44C4F0: [desc + base + (conjunto*16 + dir)*4]; 0 -> o do conjunto 0
    // da MESMA tabela. (O porte cai ainda na tabela A se a B nao tiver nada.)
    const int (*tab)[16] = isEden ? kVehicleAnim[type] : kVehicleAnimB[type];
    int a = tab[set][dir];
    if (!a) a = tab[0][dir];
    if (!a) a = kVehicleAnim[type][0][dir];
    if (!a) return -1;
    int idx = a - kAnimIndexBias;
    return idx >= 0 ? idx : -1;
}

int vehicleAnimIndex(int type, int rotation, bool isEden) {
    return vehicleAnimIndexSet(type, rotation, isEden, 0);
}

int vehicleAnimSet(int type, int action, int subState, bool movingNow) {
    if (type < 1 || type > 0x10) return 0;
    if (subState == 1 && movingNow) return 1;                   // andando (velocidade ou giro)
    if (subState != 1 && action == kVehicleWorkAction[type][0]) return 2;
    if (subState != 1 && action == kVehicleWorkAction[type][1]) return 3;
    return 0;
}

int vehicleWorkAction(int type, int which) {
    if (type < 1 || type > 0x10 || which < 0 || which > 1) return 0x35;
    return kVehicleWorkAction[type][which];
}

// Code_Name (GetName = vt[1] do descritor) por map_id 0x00..0x10.
static const char* const kVehicleCodeName[0x11] = {
    nullptr,  // 0x00
    "BIG_TRUCK",  // 0x01
    "CON_TRUCK",  // 0x02
    "SPIDER",  // 0x03
    "SCORPION",  // 0x04
    "LIGHT_TANK",  // 0x05
    "MED_TANK",  // 0x06
    "BIG_TANK",  // 0x07
    "SURVEYOR",  // 0x08
    "MINER",  // 0x09
    "GEO_METRO",  // 0x0A
    "SCOUT",  // 0x0B
    "BULL_DOZER",  // 0x0C
    "MOBILE_HOME",  // 0x0D
    "REPAIR_TRUCK",  // 0x0E
    "WALL_TRUCK",  // 0x0F
    "SCAT",  // 0x10
};

const char* vehicleCodeName(int type) {
    if (type < 0 || type > 0x10) return nullptr;
    return kVehicleCodeName[type];
}

} // namespace op2
