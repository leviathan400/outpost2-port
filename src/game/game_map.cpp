#include "game_map.h"

#include <cstdlib>
#include <cstring>

#include "../res/res_manager.h"

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

uint32_t GameMap::rawCell(int x, int y) const {
    x &= m_widthMask;                       // wrap horizontal (como o engine)
    if (y < 0) y = 0;
    if (y >= m_height) y = m_height - 1;
    // Layout real do engine (GameMap::GetTile @191591): faixas verticais de 32
    // colunas. offset_u32 = ((x>>5)*H + y)*32 + (x&31)  [H sempre potência de 2].
    // NÃO é row-major — ler row-major embaralha o mapa.
    size_t idx = (static_cast<size_t>(x >> 5) * m_height + y) * 32 + (x & 31);
    return m_tiles[idx];
}

int GameMap::getTile(LOCATION loc) const {
    return (rawCell(loc.x, loc.y) & 0xFFE0) >> 5;   // bits 5..15
}

int GameMap::getCellType(LOCATION loc) const {
    return rawCell(loc.x, loc.y) & 0x1F;            // bits 0..4
}

bool GameMap::cellTypePassable(int cellType) {
    // Impassáveis = entradas de custo 0 na tabela do engine (sub_40B500 @18551).
    // Os nomes na tabela começam em cellType 1 (cellType 0 = fastPassible1,
    // passável, setado antes). Logo: 1=Impassible2, 6=Impassible1, 8=NorthCliffs,
    // 9=CliffsHighSide, 10=CliffsLowSide. (0 e 7 são fast-passible = passáveis.)
    switch (cellType) {
    case 1:
    case 6:
    case 8:
    case 9:
    case 10:
        return false;
    default:
        return true;
    }
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
