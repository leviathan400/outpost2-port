#include "walls.h"

#include <cstdint>

#include "colony.h"
#include "game_map.h"
#include "player.h"

namespace op2 {

// dword_4EA540 / 4EA580 / 4EA5C0 / 4EA600: nova entrada do grupo do vizinho
// de cima / baixo / esquerda / direita quando ganha a ligação com (x,y).
// dword_4EA740: entrada do grupo do próprio tile pela máscara (1 cima, 2 baixo,
// 4 esquerda, 8 direita). Lidas do OUTPOST2.EXE.
static const int kUp[16]    = {7, 1, 2, 3, 9, 10, 8, 7, 8, 9, 10, 11, 1, 3, 2, 11};
static const int kDown[16]  = {6, 1, 9, 10, 4, 5, 6, 8, 8, 9, 10, 1, 12, 5, 4, 12};
static const int kLeft[16]  = {0, 10, 7, 3, 6, 5, 6, 7, 8, 8, 10, 3, 5, 13, 0, 13};
static const int kRight[16] = {0, 9, 2, 7, 4, 6, 6, 7, 8, 9, 8, 2, 4, 0, 14, 14};
static const int kCenter[16] = {15, 12, 11, 1, 14, 4, 2, 9, 13, 5, 3, 10, 0, 6, 7, 8};

// sub_4A18D0: grupo (offset no registro do tipo de terreno) e índice do tile.
static bool tileGroup(int tile, const GameMap::TerrainType*& rec, int& base, int& index) {
    for (const auto& t : g_gameMap.terrainTypes()) {
        auto in = [&](int b, int e) { return t.word(b) <= tile && t.word(e) >= tile; };
        int found = -1;
        if (t.word(0x14) > 0 && in(0x14, 0x32)) found = 0x14;
        else if (t.word(0x34) > 0 && in(0x34, 0x52)) found = 0x34;
        else if (t.word(0x54) > 0 && in(0x54, 0x72)) found = 0x54;
        else if (t.word(0x54) > 0 && in(0x74, 0x92)) found = 0x74;
        else if (t.word(0x54) > 0 && in(0x94, 0xB2)) found = 0x94;
        else if (t.word(0xBC) > 0 && in(0xBC, 0xDA)) found = 0xBC;
        if (found >= 0) { rec = &t; base = found; index = tile - t.word(found); return true; }
    }
    return false;
}

// sub_4A1C10 (e os testes em linha de sub_4A1CB0): o vizinho é do mesmo tipo?
static bool sameKind(int cellType, int wallType) {
    switch (wallType) {
        case 0x11: return cellType == 0x1A;
        case 0x12: return cellType == 0x17;
        case 0x13: return cellType == 0x19;
        case 0x14: return cellType == 0x18;
        default:   return (cellType >= 0x17 && cellType <= 0x19) || cellType == 0x1A;
    }
}

// sub_435330: troca só o tileIndex (bits 5..15) da célula.
static void setTileIndex(int x, int y, int tile) {
    const uint32_t c = g_gameMap.rawCell(x, y);
    g_gameMap.setRawCell(x, y, (c & 0xFFFF001Fu) | ((static_cast<uint32_t>(tile) << 5) & 0xFFE0u));
}

static void relinkNeighbor(int x, int y, const int* table) {
    const int tile = static_cast<int>((g_gameMap.rawCell(x, y) & 0xFFE0) >> 5);
    const GameMap::TerrainType* rec; int base, index;
    int nt = tile;
    if (tileGroup(tile, rec, base, index)) nt = rec->word(base + 2 * table[index]);
    setTileIndex(x, y, nt);
}

void autotileWallTube(int x, int y, int wallType) {
    const int mask = g_gameMap.width() - 1, H = g_gameMap.height();
    int m = 0;
    if (y > 0 && sameKind(static_cast<int>(g_gameMap.rawCell(x, y - 1) & 0x1F), wallType)) {
        m |= 1; relinkNeighbor(x, y - 1, kUp);
    }
    if (y + 1 < H && sameKind(static_cast<int>(g_gameMap.rawCell(x, y + 1) & 0x1F), wallType)) {
        m |= 2; relinkNeighbor(x, y + 1, kDown);
    }
    if (sameKind(static_cast<int>(g_gameMap.rawCell((x + 1) & mask, y) & 0x1F), wallType)) {
        m |= 8; relinkNeighbor((x + 1) & mask, y, kRight);
    }
    if (sameKind(static_cast<int>(g_gameMap.rawCell((x - 1) & mask, y) & 0x1F), wallType)) {
        m |= 4; relinkNeighbor((x - 1) & mask, y, kLeft);
    }
    int tile = static_cast<int>((g_gameMap.rawCell(x, y) & 0xFFE0) >> 5);
    const GameMap::TerrainType* rec; int base, index;
    if (m && tileGroup(tile, rec, base, index)) tile = rec->word(base + 2 * kCenter[m]);
    setTileIndex(x, y, tile);
}

int tubeTileFor(int tile) {                                   // sub_48B860
    for (const auto& t : g_gameMap.terrainTypes())
        if (t.word(0) <= tile && t.word(2) >= tile) return t.word(0xDA);
    return 0;
}

// sub_48B8B0: tile de parede isolada do tipo (0x12 → +0x72, 0x13 → +0x32,
// 0x14 → +0x52); sem tipo de terreno: 0x6C.
static int wallTileFor(int tile, int wallType) {
    for (const auto& t : g_gameMap.terrainTypes()) {
        if (t.word(0) > tile || t.word(2) < tile) continue;
        if (wallType == 0x12) return t.word(0x72);
        if (wallType == 0x13) return t.word(0x32);
        if (wallType == 0x14) return t.word(0x52);
    }
    return 0x6C;
}

int createWallOrTube(int x, int y, int wallType) {
    int cellType;
    switch (wallType) {
        case 0x11: cellType = 0x1A; break;
        case 0x12: cellType = 0x17; break;
        case 0x13: cellType = 0x19; break;
        case 0x14: cellType = 0x18; break;
        default:   cellType = y; break;     // (o original usa o y aqui)
    }
    uint32_t c = g_gameMap.rawCell(x, y);
    const int tile = static_cast<int>((c & 0xFFE0) >> 5);
    const int nt = cellType == 0x1A ? tubeTileFor(tile) : wallTileFor(tile, wallType);
    c = (c & 0xFFFF0000u) | ((static_cast<uint32_t>(nt) << 5) & 0xFFE0u) | (static_cast<uint32_t>(cellType) & 0x1F);
    if (wallType != 0x11) c |= 0x80000000u;
    g_gameMap.setRawCell(x, y, c);
    autotileWallTube(x, y, wallType);
    if (wallType == 0x11)                                    // sub_42F690: rede de tubos
        for (int p = 0; p < kMaxPlayers; ++p) colonyMarkDirty(p);
    return 1;
}

} // namespace op2
