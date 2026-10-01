#include "game_map.h"

#include <cstdlib>
#include <cstring>

#include "../res/res_manager.h"
#include "terrain_cost.h"

namespace op2 {

GameMap g_gameMap;

bool GameMap::loadFromStream(StreamIO& s) {
    return load(s.data(), s.size());
}

bool GameMap::load(const uint8_t* d, size_t size) {
    m_ok = false;
    m_tilesets.clear();
    m_tiles.clear();
    if (size < 20) return false;

    auto u32 = [&](size_t off) -> uint32_t {
        uint32_t v;
        std::memcpy(&v, d + off, 4);
        return v;
    };
    auto i32 = [&](size_t off) -> int32_t {
        int32_t v;
        std::memcpy(&v, d + off, 4);
        return v;
    };

    uint32_t tag = u32(0);
    if (tag != 0x1011) return false;   // tag de mapa
    // u32 versionTag @4 (0 = mapa)
    m_lgWidth = static_cast<int>(u32(8));
    m_height = static_cast<int>(u32(12));
    m_numTileMappings = u32(16);
    m_width = 1 << m_lgWidth;
    m_widthMask = m_width - 1;
    if (m_width <= 0 || m_height <= 0) return false;

    // Grid de tiles (u32 cada, ordem de linha).
    size_t n = static_cast<size_t>(m_width) * m_height;
    size_t tileOff = 20;
    if (tileOff + n * 4 > size) return false;
    m_tiles.resize(n);
    std::memcpy(m_tiles.data(), d + tileOff, n * 4);

    // clipRect (4 x i32): x1,y1,x2,y2
    size_t o = tileOff + n * 4;
    if (o + 16 > size) return false;
    m_clip.x1 = i32(o);
    m_clip.y1 = i32(o + 4);
    m_clip.x2 = i32(o + 8);
    m_clip.y2 = i32(o + 12);
    o += 16;

    // Tabela de tilesets: { u32 nameLen; char name[]; u32 numTiles } enquanto
    // nameLen for plausível.
    while (o + 4 <= size) {
        uint32_t nameLen = u32(o);
        if (nameLen < 1 || nameLen > 16) break;
        if (o + 4 + nameLen + 4 > size) break;
        Tileset ts;
        ts.name.assign(reinterpret_cast<const char*>(d + o + 4), nameLen);
        ts.numTiles = static_cast<int>(u32(o + 4 + nameLen));
        m_tilesets.push_back(std::move(ts));
        o += 8 + nameLen;
    }

    m_ok = true;
    return true;
}

size_t GameMap::cellIndex(int x, int y) const {
    if (m_tiles.empty()) return static_cast<size_t>(-1);
    x &= m_widthMask;                       // wrap horizontal (como o engine)
    if (y < 0) y = 0;
    if (y >= m_height) y = m_height - 1;
    // Layout real do engine (GameMap::GetTile @191591; idêntico ao índice de
    // sub_4382C0/sub_438390): faixas verticais de 32 colunas.
    //   idx_u32 = ((x>>5)*H + y)*32 + (x&31)   [H potência de 2 ⇒ == o shl do asm]
    // NÃO é row-major — ler row-major embaralha o mapa.
    return (static_cast<size_t>(x >> 5) * m_height + y) * 32 + (x & 31);
}

uint32_t GameMap::rawCell(int x, int y) const {
    size_t idx = cellIndex(x, y);
    return idx == static_cast<size_t>(-1) ? 0u : m_tiles[idx];
}

void GameMap::setUnitOccupancy(int x, int y, int unitId) {
    size_t idx = cellIndex(x, y);
    if (idx == static_cast<size_t>(-1)) return;
    uint32_t occ = (static_cast<uint32_t>(unitId) << CELL_OCC_SHIFT) & CELL_OCCUPANCY;
    m_tiles[idx] = (m_tiles[idx] & ~CELL_OCCUPANCY) | occ;
}

void GameMap::clearUnitOccupancy(int x, int y) {
    size_t idx = cellIndex(x, y);
    if (idx == static_cast<size_t>(-1)) return;
    m_tiles[idx] &= ~CELL_OCCUPANCY;
}

int GameMap::cellOccupant(int x, int y) const {
    return static_cast<int>((rawCell(x, y) & CELL_OCCUPANCY) >> CELL_OCC_SHIFT);
}

int GameMap::getTile(LOCATION loc) const {
    return (rawCell(loc.x, loc.y) & 0xFFE0) >> 5;   // bits 5..15
}

int GameMap::getCellType(LOCATION loc) const {
    return rawCell(loc.x, loc.y) & 0x1F;            // bits 0..4
}

bool GameMap::cellTypePassable(int cellType) {
    // Passabilidade = custo != 0 na tabela real do engine (dword_4DEB9C, extraída
    // de sub_40B500 @18551; ver terrain_cost). Usa a classe de movimento 0 (veículo
    // terrestre padrão). Impassáveis de terreno: cellTypes {1,6,8,9,10} (custo 0),
    // conferido empiricamente no overlay (cânions/cliffs/vents).
    return terrainPassable(cellType, 0);
}

LOCATION GameMap::findPassableNear(LOCATION start, int maxRadius) const {
    if (isPassable(start)) return start;
    for (int r = 1; r <= maxRadius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;  // só a borda do anel
                LOCATION c{ start.x + dx, start.y + dy };
                if (c.y < 0 || c.y >= m_height) continue;
                if (isPassable(c)) return c;
            }
        }
    }
    return start;
}

} // namespace op2
