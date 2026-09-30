#!/usr/bin/env python3
"""
Parser do formato de bitmap "PBMP" do Outpost 2 (tilesets well####.bmp,
armazenados dentro do MAPS.VOL). Formato chunked (IFF-like):

  "PBMP" u32 size
    "head" u32 len(=20)  { u32 ver=2; u32 width; u32 height; u32 bpp=8; u32 ? }
    "PPAL" u32 len       (paleta; contém sub-chunks)
        "head" u32 len(=4) { u32 numPalettes=1 }
        "data" u32 len(=1024) { RGBQUAD[256] : b,g,r,a }
    "data" u32 len       { pixels 8-bit, width*height, top-down }

Os tilesets são uma coluna de tiles 32x32 empilhados (height = numTiles*32).
"""
import struct
import sys


def _find_chunk(data, tag, start=0, end=None):
    """Acha o chunk `tag` varrendo os chunks a partir de `start`."""
    if end is None:
        end = len(data)
    o = start
    while o + 8 <= end:
        t = data[o:o + 4]
        ln = struct.unpack_from("<I", data, o + 4)[0]
        if t == tag:
            return o + 8, ln
        o += 8 + ln
    return None, None


class Pbmp:
    def __init__(self, data):
        self.data = data
        if data[:4] != b"PBMP":
            raise ValueError("não é PBMP")
        # head
        ho, hl = _find_chunk(data, b"head", 8)
        ver, self.width, self.height, self.bpp, _ = struct.unpack_from("<5I", data, ho)
        # PPAL -> data (paleta)
        po, pl = _find_chunk(data, b"PPAL", 8)
        pdo, pdl = _find_chunk(data, b"data", po, po + pl)
        pal = data[pdo:pdo + pdl]
        # Paleta PBMP é R,G,B,flag (convenção OP2), NÃO RGBQUAD (b,g,r) do Windows.
        # Confirmado contra o render original da wiki (lava = laranja): idx de lava
        # = (90,24,16) que só é laranja se lido como R,G,B.
        self.palette = [(pal[i], pal[i + 1], pal[i + 2]) for i in range(0, min(pdl, 1024), 4)]
        # pixels: o "data" de topo, logo após o "data" da paleta
        do, dl = _find_chunk(data, b"data", pdo + pdl)
        self.pixels = data[do:do + dl]

    @property
    def num_tiles(self):
        return self.height // 32

    def to_image(self):
        from PIL import Image
        img = Image.new("RGB", (self.width, self.height))
        px = img.load()
        pal = self.palette
        w = self.width
        for y in range(self.height):
            row = y * w
            for x in range(w):
                px[x, y] = pal[self.pixels[row + x]]
        return img

    def tile_image(self, index):
        """Extrai o tile 32x32 de índice `index` (0..num_tiles-1)."""
        from PIL import Image
        img = Image.new("RGB", (32, 32))
        px = img.load()
        base = index * 32 * self.width
        for y in range(32):
            for x in range(32):
                px[x, y] = self.palette[self.pixels[base + y * self.width + x]]
        return img


def load(volpath, name):
    sys.path.insert(0, "tools")
    from vol import VolFile
    v = VolFile(volpath)
    e = [x for x in v.entries if x.name == name][0]
    return Pbmp(v.read(e))


if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "well0001.bmp"
    p = load("MAPS.VOL", name)
    print(f"{name}: {p.width}x{p.height} bpp={p.bpp} -> {p.num_tiles} tiles 32x32, "
          f"paleta={len(p.palette)} cores")
