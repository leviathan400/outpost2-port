#include "terrain_renderer.h"

#include <algorithm>
#include <iostream>

#include "game_map.h"
#include "location.h"
#include "../res/res_manager.h"

namespace op2 {

bool TerrainRenderer::init(const GameMap& map, ResManager& res) {
    m_map = &map;
    m_tilesets.clear();
    m_pool.clear();
    m_ok = false;

    for (const auto& ts : map.tilesets()) {
        std::string file = ts.name;
        // o mapa lista os nomes sem extensão
        if (file.size() < 4 || file.substr(file.size() - 4) != ".bmp")
            file += ".bmp";
        auto stream = res.openStream(file);
        if (!stream) {
            std::cerr << "[Terrain] tileset ausente: " << file << std::endl;
            return false;
        }
        auto pbmp = std::make_unique<Pbmp>();
        if (!pbmp->load(stream->data(), stream->size())) {
            std::cerr << "[Terrain] PBMP inválido: " << file << std::endl;
            return false;
        }
        int tsIndex = static_cast<int>(m_tilesets.size());
        int nt = pbmp->numTiles();
        for (int i = 0; i < nt; ++i) m_pool.push_back({tsIndex, i});
        m_tilesets.push_back(std::move(pbmp));
    }

    m_ok = true;
    return true;
}

int TerrainRenderer::mapPixelWidth()  const { return m_map ? m_map->width()  * 32 : 0; }
int TerrainRenderer::mapPixelHeight() const { return m_map ? m_map->height() * 32 : 0; }


void TerrainRenderer::render(uint32_t* dst, int dstW, int dstH, int camX, int camY,
                             int (*darkOf)(int), bool wrapX) const {
    if (!m_ok || !m_map) return;

    // Faixa de tiles que toca a viewport (usa tx/ty reais; getTile faz wrap em X).
    int firstTx = camX >= 0 ? camX / 32 : (camX - 31) / 32;
    int firstTy = camY >= 0 ? camY / 32 : (camY - 31) / 32;
    int lastTx = (camX + dstW - 1) >= 0 ? (camX + dstW - 1) / 32 : (camX + dstW - 32) / 32;
    int lastTy = (camY + dstH - 1) >= 0 ? (camY + dstH - 1) / 32 : (camY + dstH - 32) / 32;

    int h = m_map->height();

    for (int ty = firstTy; ty <= lastTy; ++ty) {
        if (ty < 0 || ty >= h) continue;   // sem wrap vertical
        for (int tx = firstTx; tx <= lastTx; ++tx) {
            if (!wrapX && (tx < 0 || tx >= m_map->width())) continue;
            int g = m_map->getTile(LOCATION{tx, ty});
            if (g < 0 || g >= static_cast<int>(m_pool.size())) continue;
            const PoolEntry& pe = m_pool[g];
            const Pbmp* ts = m_tilesets[pe.tileset].get();
            const uint8_t* idx = ts ? ts->tileIndices(pe.local) : nullptr;
            if (!idx) continue;
            // A escuridao e por COLUNA DE TILE, uma paleta por blit.
            const uint32_t* pal = ts->palette(darkOf ? darkOf(tx) : 0);
            const int idxStride = ts->indexStride();

            int screenX = tx * 32 - camX;   // canto do tile na tela
            int screenY = ty * 32 - camY;

            // Sub-retângulo visível do tile.
            int sx0 = std::max(0, -screenX);
            int sy0 = std::max(0, -screenY);
            int sx1 = std::min(32, dstW - screenX);
            int sy1 = std::min(32, dstH - screenY);
            if (sx0 >= sx1 || sy0 >= sy1) continue;

            for (int sy = sy0; sy < sy1; ++sy) {
                const uint8_t* srcRow = idx + static_cast<size_t>(sy) * idxStride;
                uint32_t* dstRow = dst + static_cast<size_t>(screenY + sy) * dstW + (screenX + sx0);
                for (int sx = sx0; sx < sx1; ++sx) *dstRow++ = pal[srcRow[sx]];
            }
        }
    }
}

uint16_t TerrainRenderer::minimapColor(int mappingIndex) const {
    if (m_minimapColors.empty() && m_map) {
        auto& mc = const_cast<std::vector<uint16_t>&>(m_minimapColors);
        const auto& maps = m_map->tileMappings();
        mc.assign(maps.size(), 0);
        std::vector<uint32_t> buf(32 * 32);
        for (size_t i = 0; i < maps.size(); ++i) {
            const int ts = m_map->tilesetOfSlot(maps[i].tileset);
            if (ts < 0 || ts >= static_cast<int>(m_tilesets.size()) || !m_tilesets[ts]) continue;
            if (!m_tilesets[ts]->blitTile(maps[i].tile, buf.data(), 32)) continue;
            // Soma em 5 bits por canal (paleta RGB555 do tileset) e média /1024.
            int r = 0, g = 0, b = 0;
            for (uint32_t c : buf) {
                r += (c >> 16) & 0xF8; g += (c >> 8) & 0xF8; b += c & 0xF8;
            }
            r = (r >> 10) & 0xF8; g = (g >> 10) & 0xF8; b = (b >> 10) & 0xF8;
            mc[i] = static_cast<uint16_t>((r << 7) | (g << 2) | (b >> 3));
        }
    }
    return (mappingIndex >= 0 && mappingIndex < static_cast<int>(m_minimapColors.size()))
               ? m_minimapColors[mappingIndex] : 0;
}

} // namespace op2
