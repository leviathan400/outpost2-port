#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "location.h"

namespace op2 {

class StreamIO;

// Mapa de terreno do jogo (porta GameMap, asm @191591; ver [[op2-map-format]] e
// [[op2-engine-architecture]]). No engine GameMap é um singleton de métodos
// static sobre globais (tile base, widthMask, lgWidth); aqui é uma instância com
// um global g_gameMap espelhando esse desenho.
//
// Mecanismo real de acesso extraído do disassembly:
//   GetTile(x,y)     = (cell & 0xFFE0) >> 5   (tileIndex: bits 5..15, 11 bits)
//   GetCellType(x,y) =  cell & 0x1F           (bits 0..4)
//   x é mascarado por widthMask (width-1)     (mapas dão wrap horizontal em X)
class GameMap {
public:
    struct Tileset {
        std::string name;   // ex.: "well0001" (sem extensão)
        int numTiles = 0;
    };

    // Carrega a partir dos bytes de um .map (conteúdo de um recurso do VOL).
    bool load(const uint8_t* data, size_t size);
    bool loadFromStream(StreamIO& s);
    bool ok() const { return m_ok; }

    int width()  const { return m_width; }
    int height() const { return m_height; }
    int lgWidth() const { return m_lgWidth; }
    const MAP_RECT& clipRect() const { return m_clip; }
    const std::vector<Tileset>& tilesets() const { return m_tilesets; }

    // Acesso fiel (com máscaras e wrap em X). y fora de faixa é clampeado.
    int getTile(LOCATION loc) const;
    int getCellType(LOCATION loc) const;

    // Passabilidade: um cellType é impassável quando sua entrada na tabela de
    // custo do engine (sub_40B500 @asm 18551) tem custo 0. São eles: 0=Impassible2,
    // 5=Impassible1, 7=NorthCliffs, 8=CliffsHigh, 9=CliffsLow. Verificado contra o
    // terreno (montanhas/cânions). Base para colocação de unidades e pathfinding.
    static bool cellTypePassable(int cellType);
    bool isPassable(LOCATION loc) const { return cellTypePassable(getCellType(loc)); }

    // Acha a célula passável mais próxima de `start` (busca em espiral); devolve
    // `start` se nada for achado dentro de maxRadius.
    LOCATION findPassableNear(LOCATION start, int maxRadius = 16) const;

    // Célula crua (u32) em (x,y) com wrap em X.
    uint32_t rawCell(int x, int y) const;

private:
    bool m_ok = false;
    int m_width = 0;
    int m_height = 0;
    int m_lgWidth = 0;
    int m_widthMask = 0;
    uint32_t m_numTileMappings = 0;
    MAP_RECT m_clip;
    std::vector<uint32_t> m_tiles;   // row-major (ordem de disco), width*height
    std::vector<Tileset> m_tilesets;
};

// Instância global, como o mapa singleton do engine.
extern GameMap g_gameMap;

} // namespace op2
