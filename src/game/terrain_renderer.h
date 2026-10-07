#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../gfx/pbmp.h"

namespace op2 {

class GameMap;
class ResManager;

// Renderiza o terreno de um GameMap com os gráficos reais dos tilesets (PBMP).
// O tileIndex de cada célula (GameMap::getTile) indexa o POOL GLOBAL de tiles,
// formado concatenando, na ordem listada no mapa, os tiles 32x32 de cada tileset
// (ver [[op2-pbmp-format]] e tools/render_map.py, validado).
class TerrainRenderer {
public:
    // Carrega os tilesets do mapa via ResManager. false se algum faltar.
    bool init(const GameMap& map, ResManager& res);
    bool ok() const { return m_ok; }

    int mapPixelWidth()  const;
    int mapPixelHeight() const;

    // Desenha a região visível no buffer ARGB `dst` (dstW x dstH, stride=dstW).
    // (camX,camY) = pixel do canto superior-esquerdo da câmera no espaço do mapa.
    // Faz wrap horizontal (como o engine). Células sem tile no pool ficam pretas.
    void render(uint32_t* dst, int dstW, int dstH, int camX, int camY) const;

    // Cor do minimapa de um tile (RGB555): média dos 1024 pixels do tile do
    // mapeamento, em 5 bits por canal — sub_48B320 (tabela [obj+0x46C]->[+8]).
    uint16_t minimapColor(int mappingIndex) const;

private:
    struct PoolEntry { int tileset; int local; };

    const GameMap* m_map = nullptr;
    std::vector<std::unique_ptr<Pbmp>> m_tilesets;
    std::vector<PoolEntry> m_pool;                 // globalIndex -> (tileset, local)
    mutable std::unordered_map<int, std::vector<uint32_t>> m_cache; // tile 32x32 ARGB
    std::vector<uint16_t> m_minimapColors;         // por mapeamento (sub_48B2E0)
    bool m_ok = false;

    const std::vector<uint32_t>* tilePixels(int globalIndex) const;
};

} // namespace op2
