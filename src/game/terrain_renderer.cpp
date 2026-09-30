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
    m_cache.clear();
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

const std::vector<uint32_t>* TerrainRenderer::tilePixels(int g) const {
    if (g < 0 || g >= static_cast<int>(m_pool.size())) return nullptr;
    auto it = m_cache.find(g);
    if (it != m_cache.end()) return &it->second;
    const PoolEntry& pe = m_pool[g];
    std::vector<uint32_t> buf(32 * 32, 0xFF000000u);
    if (!m_tilesets[pe.tileset]->blitTile(pe.local, buf.data(), 32)) return nullptr;
    auto res = m_cache.emplace(g, std::move(buf));
    return &res.first->second;
}

void TerrainRenderer::render(uint32_t* dst, int dstW, int dstH, int camX, int camY) const {
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
            int g = m_map->getTile(LOCATION{tx, ty});
            const std::vector<uint32_t>* tile = tilePixels(g);
            if (!tile) continue;

            int screenX = tx * 32 - camX;   // canto do tile na tela
            int screenY = ty * 32 - camY;

            // Sub-retângulo visível do tile.
            int sx0 = std::max(0, -screenX);
            int sy0 = std::max(0, -screenY);
            int sx1 = std::min(32, dstW - screenX);
            int sy1 = std::min(32, dstH - screenY);
            if (sx0 >= sx1 || sy0 >= sy1) continue;

            for (int sy = sy0; sy < sy1; ++sy) {
                const uint32_t* srcRow = tile->data() + sy * 32;
                uint32_t* dstRow = dst + static_cast<size_t>(screenY + sy) * dstW + (screenX + sx0);
                std::copy(srcRow + sx0, srcRow + sx1, dstRow);
            }
        }
    }
}

} // namespace op2
