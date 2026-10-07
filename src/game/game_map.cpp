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

    // Tabela de tilesets (sub_435CC0 @90244): são EXATAMENTE `numTilesets` slots
    // (header @16, ex. 512). Cada slot = string (sub_49DAF0: u32 len + chars); se
    // len != 0, segue u32 numTiles. Slots vazios são só o u32 0.
    m_slotToTileset.assign(m_numTileMappings, -1);
    for (uint32_t slot = 0; slot < m_numTileMappings; ++slot) {
        if (o + 4 > size) return false;
        uint32_t nameLen = u32(o); o += 4;
        if (nameLen == 0) continue;
        m_slotToTileset[slot] = static_cast<int>(m_tilesets.size());
        if (o + nameLen + 4 > size) return false;
        Tileset ts;
        ts.name.assign(reinterpret_cast<const char*>(d + o), nameLen);
        ts.numTiles = static_cast<int>(u32(o + nameLen));
        m_tilesets.push_back(std::move(ts));
        o += nameLen + 4;
    }

    // Seção "TILE SET" (sub_48B410 @227950): tag de 10 bytes "TILE SET\x1A\0",
    // u32 numMappings + registros de 8 bytes, u32 numTerrainTypes + registros de
    // 0x108 bytes. O objeto do engine (0x40 bytes, sub_48B270) guarda até 12
    // ponteiros de tipo de terreno em +0x10.
    m_terrainTypes.clear();
    static const char kTag[10] = {'T','I','L','E',' ','S','E','T','\x1A','\0'};
    if (o + 10 + 4 <= size && std::memcmp(d + o, kTag, 10) == 0) {
        o += 10;
        uint32_t numMappings = u32(o); o += 4;
        m_mappings.clear();
        for (uint32_t k = 0; k < numMappings && o + 8 <= size; ++k, o += 8) {
            TileMapping tm;
            std::memcpy(&tm.tileset, d + o, 2);
            std::memcpy(&tm.tile, d + o + 2, 2);
            m_mappings.push_back(tm);        // (+4..+7: animação do tile)
        }
        if (o + 4 <= size) {
            uint32_t numTypes = u32(o); o += 4;
            for (uint32_t t = 0; t < numTypes && t < kMaxTerrainTypes; ++t) {
                if (o + 0x108 > size) break;
                TerrainType tt;
                std::memcpy(tt.raw, d + o, 0x108);
                // Pós-processo do loader: se a lista em +0xBC está vazia e +8 != 0,
                // preenche +0xBC..+0xDA com (+8)+0..15.
                uint16_t w8, wBC;
                std::memcpy(&w8, tt.raw + 8, 2);
                std::memcpy(&wBC, tt.raw + 0xBC, 2);
                if (wBC == 0 && w8 != 0)
                    for (int k = 0; k < 16; ++k) {
                        uint16_t v = static_cast<uint16_t>(w8 + k);
                        std::memcpy(tt.raw + 0xBC + k * 2, &v, 2);
                    }
                m_terrainTypes.push_back(tt);
                o += 0x108;
            }
        }
    }

    // Largura interna + curva de luz (sub_4353F0 @89229 / loop @loc_435634).
    if (m_lgWidth < 9) { m_internalWidth = m_width * 2; m_internalXOff = 32; }
    else { m_internalWidth = m_width; m_internalXOff = 0; }
    m_light.assign(m_internalWidth, 0);
    m_light2.assign(m_internalWidth, 0);
    for (int x = 0; x < m_internalWidth; ++x) {
        const int v = static_cast<int>((static_cast<int64_t>(x) << 16) / m_internalWidth);
        if (v >= 0x4000 && v <= 0xC000) { m_light[x] = 0x1F; m_light2[x] = 0x16; continue; }
        int a = static_cast<int16_t>(v);
        if (a < 0) a = -a;
        a = (a & 0x3E00) >> 9;
        if (a > 30) a = 30;
        if (a < 0) a = 0;
        m_light[x] = static_cast<uint8_t>(a);
        m_light2[x] = static_cast<uint8_t>(a - 9 > 0 ? a - 9 : 0);
    }

    m_ok = true;
    return true;
}

int GameMap::TerrainType::word(int off) const {
    int16_t v; std::memcpy(&v, raw + off, 2); return v;
}

// sub_48B6A0: tipo de terreno cuja faixa [+0,+2] contém o tile → tile
// "terraplanado" (+4). Sem tipo: 0x0A.
int GameMap::bulldozedTile(int tile) const {
    for (const auto& t : m_terrainTypes)
        if (t.word(0) <= tile && t.word(2) >= tile) return t.word(4);
    return 0x0A;
}

// sub_48B7A0: entulho COMUM = (+6) + rand(4). O sorteio vem ANTES da busca (o
// RNG avança mesmo sem tipo); sem tipo: 0x35.
int GameMap::commonRubbleTile(int tile, int rnd4) const {
    for (const auto& t : m_terrainTypes)
        if (t.word(0) <= tile && t.word(2) >= tile) return t.word(6) + rnd4;
    return 0x35;
}

// sub_48B800: entulho RARO = (+6) + rand(4) + 4; sem tipo: 0x35.
int GameMap::rareRubbleTile(int tile, int rnd4) const {
    for (const auto& t : m_terrainTypes)
        if (t.word(0) <= tile && t.word(2) >= tile) return t.word(6) + rnd4 + 4;
    return 0x35;
}

void GameMap::setTileAndCellType(int x, int y, int tile, int cellType) {
    size_t idx = cellIndex(x, y);
    if (idx == static_cast<size_t>(-1)) return;
    // Mesma escrita do sub_4831A0: mantém os bits altos (0xFFFF0000), troca o
    // tileIndex (bits 5..15) e o cellType (bits 0..4).
    m_tiles[idx] = (m_tiles[idx] & 0xFFFF0000u)
                 | ((static_cast<uint32_t>(tile) << 5) & 0xFFE0u)
                 | (static_cast<uint32_t>(cellType) & 0x1Fu);
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

LOCATION GameMap::findFreeNear(LOCATION start, int maxRadius) const {
    auto free = [&](LOCATION c) {
        return c.y >= 0 && c.y < m_height && isPassable(c) &&
               cellOccupant(c.x, c.y) == 0;
    };
    if (free(start)) return start;
    for (int r = 1; r <= maxRadius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;  // borda do anel
                LOCATION c{ start.x + dx, start.y + dy };
                if (free(c)) return c;
            }
        }
    }
    return start;
}

} // namespace op2
