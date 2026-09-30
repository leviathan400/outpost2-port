#!/usr/bin/env python3
"""
Parser do sistema de sprites do Outpost 2: op2_art.prt (metadados, dentro do
MAPS.VOL) + OP2_ART.BMP (atlas de pixels 8-bit, na raiz).

Confirmado no disassembly (sub_404360 = carregador "op2_art"):
  op2_art.prt:
    "CPAL" u32 numPalettes(=9)
    numPalettes x "PPAL" { "head"{u32=1}; "data"{ RGB[256]: r,g,b,flag } }
    u32 numImages
    ImageMeta[numImages] (20 bytes cada):
       u32 scanLineByteWidth   (largura em bytes, múltiplo de 4)
       u32 pixelDataOffset     (offset nos pixels do OP2_ART.BMP)
       u32 height
       u32 width
       u16 type
       u16 paletteIndex        (qual das paletas usar)
    (depois: definições de animação/grupos — TODO)

  Cada imagem: pixels 8-bit indexados, top-down, empacotados sequencialmente no
  OP2_ART.BMP a partir do seu offset de pixels (a imagem i+1 começa onde a i
  termina: offset[i+1] = offset[i] + scanLineByteWidth[i]*height[i]).
"""
import struct
import sys


class ImageMeta:
    __slots__ = ("stride", "offset", "height", "width", "type", "pal")

    def __init__(self, stride, offset, height, width, typ, pal):
        self.stride = stride
        self.offset = offset
        self.height = height
        self.width = width
        self.type = typ
        self.pal = pal


class ArtFile:
    def __init__(self, prt_data, bmp_data):
        self.prt = prt_data
        # pixels do OP2_ART.BMP: começam no offset declarado no header BMP
        pix_off = struct.unpack_from("<I", bmp_data, 10)[0]
        self.bmp_pixels = bmp_data[pix_off:]
        self._parse()

    def _u32(self, o):
        return struct.unpack_from("<I", self.prt, o)[0]

    def _parse(self):
        d = self.prt
        assert d[:4] == b"CPAL", "não é op2_art.prt"
        npal = self._u32(4)
        # paletas
        self.palettes = []
        o = 8
        for _ in range(npal):
            assert d[o:o + 4] == b"PPAL"
            # sub-chunk 'data' com 1024 bytes de paleta
            do = d.index(b"data", o) + 8
            pal = [(d[do + i], d[do + i + 1], d[do + i + 2])  # r, g, b
                   for i in range(0, 1024, 4)]
            self.palettes.append(pal)
            # próximo PPAL: fim do 'data' da paleta
            o = do + 1024

        # numImages logo após as paletas
        self.num_images = self._u32(o)
        o += 4
        self.images = []
        for _ in range(self.num_images):
            stride, offset, height, width = struct.unpack_from("<4I", d, o)
            typ, pal = struct.unpack_from("<2H", d, o + 16)
            self.images.append(ImageMeta(stride, offset, height, width, typ, pal))
            o += 20
        self.after_images_off = o

    def image_pixels(self, i):
        """Bytes 8-bit (índices de paleta) de uma imagem, sem padding."""
        m = self.images[i]
        out = bytearray(m.width * m.height)
        for y in range(m.height):
            src = m.offset + y * m.stride
            out[y * m.width:(y + 1) * m.width] = self.bmp_pixels[src:src + m.width]
        return out

    def image_to_pil(self, i, pal_index=None):
        from PIL import Image
        m = self.images[i]
        pal = self.palettes[pal_index if pal_index is not None else m.pal]
        img = Image.new("RGB", (m.width, m.height))
        px = img.load()
        pix = self.image_pixels(i)
        for y in range(m.height):
            for x in range(m.width):
                px[x, y] = pal[pix[y * m.width + x]]
        return img


def load(vol_path="MAPS.VOL", bmp_path="OP2_ART.BMP"):
    sys.path.insert(0, "tools")
    from vol import VolFile
    v = VolFile(vol_path)
    prt = v.read([x for x in v.entries if x.name == "op2_art.prt"][0])
    with open(bmp_path, "rb") as f:
        bmp = f.read()
    return ArtFile(prt, bmp)


if __name__ == "__main__":
    a = load()
    print(f"{len(a.palettes)} paletas, {a.num_images} imagens")
    print("primeiras imagens:")
    for i in range(6):
        m = a.images[i]
        print(f"  [{i}] {m.width}x{m.height} stride={m.stride} "
              f"off={m.offset} type={m.type} pal={m.pal}")
    tot = sum(m.stride * m.height for m in a.images)
    print(f"total de bytes de pixel referenciados: {tot} "
          f"(atlas tem {len(a.bmp_pixels)})")
