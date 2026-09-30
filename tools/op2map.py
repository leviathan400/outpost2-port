#!/usr/bin/env python3
"""
Parser do formato de mapa .map do Outpost 2 (tag 0x00001011).

Estrutura (deduzida dos bytes reais + disassembly, validada em vários mapas):

  Header (5 x u32 = 20 bytes):
    u32 tag            = 0x00001011
    u32 versionTag     = 0   (0 = mapa; savegames usam outro caminho)
    u32 lgWidth        log2 da largura em tiles  -> width = 1 << lgWidth
    u32 height         altura em tiles
    u32 numTileMappings  quantidade de "tile mappings" (tileInfo), ex.: 512

  TileData [width * height] (u32 cada), em ordem de linha:
    bits 0-4  : cellType (tipo de célula / passabilidade; 0-31)
    bits 5-31 : tileIndex (índice no pool global de tiles de todos os tilesets)

  clipRect (4 x i32): x1, y1, x2, y2  (área jogável; x1 costuma ser 32)

  Tabela de tilesets (fontes de tiles .bmp, ex.: "well0000"):
    repete { u32 nameLen; char name[nameLen]; u32 numTiles }
    até a soma de numTiles atingir numTileMappings.

  (Depois: "TILE SET" + tile mappings + tile groups/terrenos — TODO.)
"""
import struct
import sys


class Op2Map:
    def __init__(self, data):
        self.data = data
        self._parse()

    def _u32(self, o):
        return struct.unpack_from("<I", self.data, o)[0]

    def _parse(self):
        d = self.data
        self.tag, self.version, self.lg_width, self.height, self.num_mappings = \
            struct.unpack_from("<5I", d, 0)
        if self.tag != 0x1011:
            raise ValueError(f"tag inesperada 0x{self.tag:X}")
        self.width = 1 << self.lg_width

        # tiles
        self.tile_off = 20
        n = self.width * self.height
        self.tiles = struct.unpack_from(f"<{n}I", d, self.tile_off)

        # clipRect
        o = self.tile_off + n * 4
        self.clip = struct.unpack_from("<4i", d, o)
        o += 16

        # tabela de tilesets: { u32 nameLen; char name[]; u32 numTiles }
        # lê enquanto o nameLen for plausível (a lista termina em padding/0).
        self.tilesets = []
        total = 0
        while o + 4 < len(d):
            name_len = self._u32(o)
            if not (1 <= name_len <= 16):
                break
            name = d[o + 4:o + 4 + name_len].decode("latin-1")
            num_tiles = self._u32(o + 4 + name_len)
            self.tilesets.append((name, num_tiles))
            total += num_tiles
            o += 8 + name_len
        self.tileset_tiles_total = total
        self.after_tilesets_off = o

    # Célula (x,y) no LAYOUT REAL do engine (GameMap::GetTile @asm 191591): os
    # tiles são armazenados em FAIXAS verticais de 32 colunas, NÃO row-major.
    #   idx_u32 = ((x>>5)*H + y)*32 + (x & 31)   (H sempre potência de 2)
    # x dá wrap horizontal (widthMask = width-1).
    def cell(self, x, y):
        x &= (self.width - 1)
        idx = ((x >> 5) * self.height + y) * 32 + (x & 31)
        return self.tiles[idx]

    # helpers de célula (aplicam sobre o valor u32 de uma célula)
    @staticmethod
    def cell_type(v):
        return v & 0x1F          # bits 0..4

    @staticmethod
    def tile_index(v):
        return (v & 0xFFE0) >> 5  # bits 5..15 (11 bits), como GetTile

    def render_png(self, path, mode="tile"):
        """Salva um PNG do grid. mode='tile' (índice) ou 'cell' (passabilidade)."""
        from PIL import Image
        w, h = self.width, self.height
        img = Image.new("RGB", (w, h))
        px = img.load()
        # paleta simples por cellType
        cell_colors = {
            0: (60, 60, 70), 2: (120, 90, 60), 3: (150, 120, 80),
            4: (100, 80, 55), 5: (90, 70, 50), 6: (70, 90, 120),
            7: (40, 40, 90), 8: (30, 30, 30), 9: (180, 60, 40),
            10: (200, 200, 60),
        }
        for y in range(h):
            for x in range(w):
                v = self.cell(x, y)
                if mode == "cell":
                    px[x, y] = cell_colors.get(self.cell_type(v), (255, 0, 255))
                else:
                    ti = self.tile_index(v)
                    px[x, y] = (ti & 0xFF, (ti >> 3) & 0xFF, (ti >> 6) & 0xFF)
        img.save(path)
        return path

    def summary(self):
        print(f"tag=0x{self.tag:X} ver={self.version}  {self.width}x{self.height} "
              f"tiles  numMappings={self.num_mappings}")
        print(f"clipRect={self.clip}  (jogável {self.clip[2]-self.clip[0]}x"
              f"{self.clip[3]-self.clip[1]})")
        print(f"tilesets ({len(self.tilesets)}, soma numTiles={self.tileset_tiles_total}):")
        for name, nt in self.tilesets:
            print(f"    {name:12} {nt} tiles")
        # sanidade dos índices de tile
        idxs = [self.tile_index(t) for t in self.tiles]
        cts = [self.cell_type(t) for t in self.tiles]
        print(f"tileIndex: min={min(idxs)} max={max(idxs)}  (numMappings={self.num_mappings})")
        print(f"cellType : min={min(cts)} max={max(cts)}  distintos={sorted(set(cts))}")


def load(volpath, name):
    sys.path.insert(0, "tools")
    from vol import VolFile
    v = VolFile(volpath)
    e = [x for x in v.entries if x.name == name][0]
    return Op2Map(v.read(e))


if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "eden01.map"
    m = load("MAPS.VOL", name)
    m.summary()
