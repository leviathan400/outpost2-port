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
    // Escreve a célula crua (u32) em (x,y) com wrap em X (sub_435330 & cia.).
    void setRawCell(int x, int y, uint32_t v);

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

    // ---- Tipos de terreno (seção "TILE SET" do .map, sub_48B410) ------------
    // Registro de 0x108 bytes; campos usados: +0 início e +2 fim da faixa de
    // tiles, +4 tile "terraplanado", +6 base do entulho (4 comuns + 4 raros).
    struct TerrainType {
        uint8_t raw[0x108] = {};
        int word(int off) const;
    };
    static constexpr int kMaxTerrainTypes = 12;   // objeto 0x40: 12 ponteiros em +0x10
    const std::vector<TerrainType>& terrainTypes() const { return m_terrainTypes; }
    int bulldozedTile(int tile) const;                  // sub_48B6A0
    int commonRubbleTile(int tile, int rnd4) const;     // sub_48B7A0 (rnd4 = GetRand(4))
    int rareRubbleTile(int tile, int rnd4) const;       // sub_48B800
    // cellType 0x15 = terraplanado, 0x16 = entulho (escritos por sub_4831A0).
    // ---- Largura INTERNA do engine e curva de luz ---------------------------
    // sub_4353F0: mapas com lgWidth < 9 são guardados com a largura DOBRADA e o
    // mapa real a partir da coluna 32 (x+32); a curva de luz cobre a largura
    // interna: escuridão 0 (claro) .. 31 por coluna (byte_54EC14 = obj+0x5C) e
    // uma 2ª curva (obj+0x25C).
    int internalWidth() const { return m_internalWidth; }
    int internalXOffset() const { return m_internalXOff; }
    int lightDarkness(int internalX) const { return m_light[internalX & (m_internalWidth - 1)]; }
    // Registro de mapeamento de tile (seção "TILE SET", 8 bytes): +0 tileset
    // (slot), +2 tile dentro do tileset. O tileIndex da célula indexa estes.
    struct TileMapping { int16_t tileset = 0, tile = 0; };
    const std::vector<TileMapping>& tileMappings() const { return m_mappings; }
    // Slot de tileset (0..numTilesets-1) → índice em tilesets() (-1 se vazio).
    int tilesetOfSlot(int slot) const {
        return (slot >= 0 && slot < static_cast<int>(m_slotToTileset.size())) ? m_slotToTileset[slot] : -1;
    }

    static constexpr int CELLTYPE_DOZED  = 0x15;
    static constexpr int CELLTYPE_RUBBLE = 0x16;
    void setTileAndCellType(int x, int y, int tile, int cellType);
    // Jogo salvo: células vivas (ocupação, muros, terraplenagem) e a luz.
    void serializeRuntime(class SaveStream& s);

private:
    std::vector<TerrainType> m_terrainTypes;
    std::vector<TileMapping> m_mappings;
    std::vector<int> m_slotToTileset;
    int m_internalWidth = 0, m_internalXOff = 0;
    std::vector<uint8_t> m_light, m_light2;
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
