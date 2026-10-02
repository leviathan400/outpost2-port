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

    // Como findPassableNear, mas também exige a célula DESOCUPADA (cellOccupant==0)
    // — p/ colocar unidades sem sobrepor footprints de prédios / outras unidades.
    LOCATION findFreeNear(LOCATION start, int maxRadius = 16) const;

    // ---- Célula runtime (grid de simulação) --------------------------------
    // No engine a célula é UMA u32 (dword_54F01C aponta para o grid): os bits
    // baixos vêm do disco (cellType 0..4, tileIndex 5..15) e os bits altos
    // guardam o estado dinâmico. Layout confirmado em sub_4382C0/sub_438390 @asm:
    //   bits 0..4   cellType
    //   bits 5..15  tileIndex
    //   bits 16..26 ocupação (id da unidade ocupante, 0 = livre) — 0x07FF0000
    //   bit  27     0x08000000  \  parte de HARD-BLOCK (0x28000000): intransponível
    //   bit  29     0x20000000  /  (parede/estrutura sólida)
    //   bit  31     0x80000000  marca "estrutura/tubo": exige teste de cellType
    //   (0xA8000000 = qualquer dos bits 31/29/27 → sai do caminho de terreno)
    static constexpr uint32_t CELL_OCCUPANCY = 0x07FF0000u;  // bits 16..26
    static constexpr uint32_t CELL_WALLBITS  = 0xA8000000u;  // bits 31|29|27
    static constexpr uint32_t CELL_HARDBLOCK = 0x28000000u;  // bits 29|27
    static constexpr uint32_t CELL_STRUCT    = 0x80000000u;  // bit 31
    static constexpr int      CELL_OCC_SHIFT = 16;
    // cellTypes de tubo (passáveis via dword_4E0C08) — faixa 0x17..0x19.
    static constexpr int TUBE_CELLTYPE_LO = 0x17;
    static constexpr int TUBE_CELLTYPE_HI = 0x19;

    // Célula crua (u32) em (x,y) com wrap em X. (leitura)
    uint32_t rawCell(int x, int y) const;

    // Índice linear da célula (x,y) no grid, no layout de faixa do engine:
    //   idx = ((x>>5)*H + y)*32 + (x&31)   [H potência de 2 ⇒ == o shl do asm]
    // y é clampeado; x faz wrap por widthMask. -1 se o grid não está carregado.
    size_t cellIndex(int x, int y) const;

    // Escreve/lê a ocupação por unidade (bits 16..26). setUnitOccupancy grava o
    // id (mascara em 11 bits); clear zera; cellOccupant devolve o id (0 = livre).
    void setUnitOccupancy(int x, int y, int unitId);
    void clearUnitOccupancy(int x, int y);
    int  cellOccupant(int x, int y) const;

    // Flags de célula (bits altos).
    bool cellHardBlocked(int x, int y) const { return (rawCell(x, y) & CELL_HARDBLOCK) != 0; }
    bool cellHasStruct(int x, int y) const { return (rawCell(x, y) & CELL_WALLBITS) != 0; }

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
