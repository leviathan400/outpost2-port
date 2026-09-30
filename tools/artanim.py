#!/usr/bin/env python3
"""
Parser da seção de ANIMAÇÕES do op2_art.prt (após o array de ImageMeta).

Estrutura confirmada no disassembly (sub_404360, loop a partir de loc_4047DF) e
validada por consumo exato (frames=24185, pictures=160922 batem com o header):

  u32 numAnimations      (2079)
  u32 numFrames          (24185)  -> tamanho do array de frames (14 bytes/rec)
  u32 numPictures        (160922) -> array de pictures/layers (6 bytes/rec)
  u32 numFrameExtra      (6012)   -> array de dados extra de frame (4 bytes/rec)

  Animation[numAnimations]:
     u32 header[9]        (o último, header[8], é o numFrames desta animação;
                           header[3..6] parecem ser o bounding box)
     Frame[numFrames]:
        u8 b1  -> numPictures = b1 & 0x7F ; flag1 = b1 & 0x80
        u8 b2  -> flag2 = b2 & 0x80
        if flag1: u16 (offset de sombra/velocidade -> array de 4 bytes)
        if flag2: u16
        Picture[numPictures]:
           u16 imageIndex   (índice na tabela de imagens; 0..numImages-1)
           u8  ?            (flag por-picture, talvez espelhamento)
           u8  slot         (ordem de desenho / camada)
           i16 x            (offset em pixels)
           i16 y
     u32 extraCount
     ExtraRecord[extraCount]: 4 x u32   (eventos/timing por animação)

  (Após todas as animações há ~12KB de uma tabela auxiliar final — TODO.)
"""
import struct
import sys


class _R:
    def __init__(self, buf, o):
        self.b = buf
        self.o = o

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.o)[0]; self.o += 4; return v

    def u16(self):
        v = struct.unpack_from("<H", self.b, self.o)[0]; self.o += 2; return v

    def i16(self):
        v = struct.unpack_from("<h", self.b, self.o)[0]; self.o += 2; return v

    def u8(self):
        v = self.b[self.o]; self.o += 1; return v


class Picture:
    __slots__ = ("image", "flag", "slot", "x", "y")

    def __init__(self, image, flag, slot, x, y):
        self.image = image
        self.flag = flag
        self.slot = slot
        self.x = x
        self.y = y


def parse_animations(prt_data, start_offset):
    """Lê a seção de animações. Retorna lista de animações:
    cada uma = (header:list[9], frames:list[list[Picture]])."""
    r = _R(prt_data, start_offset)
    num_anim, num_frames, num_pics, num_extra = (r.u32(), r.u32(), r.u32(), r.u32())
    anims = []
    for _ in range(num_anim):
        header = [r.u32() for _ in range(9)]
        frames = []
        for _ in range(header[8]):
            b1 = r.u8(); npic = b1 & 0x7F; f1 = b1 & 0x80
            b2 = r.u8(); f2 = b2 & 0x80
            if f1:
                r.u16()
            if f2:
                r.u16()
            pics = [Picture(r.u16(), r.u8(), r.u8(), r.i16(), r.i16())
                    for _ in range(npic)]
            frames.append(pics)
        extra_count = r.u32()
        for _ in range(extra_count):
            r.u32(); r.u32(); r.u32(); r.u32()
        anims.append((header, frames))
    return anims, r.o


if __name__ == "__main__":
    sys.path.insert(0, "tools")
    from artfile import load
    a = load()
    anims, end = parse_animations(a.prt, a.after_images_off)
    tot_f = sum(len(f) for _, f in anims)
    tot_p = sum(len(p) for _, f in anims for p in f)
    print(f"{len(anims)} animações, {tot_f} frames, {tot_p} pictures")
    print(f"consumiu até 0x{end:X} de 0x{len(a.prt):X} "
          f"(seção auxiliar final: {len(a.prt) - end} bytes)")
    # exemplo
    h, f = anims[192]
    print(f"\nanim 192: header={h}, {len(f)} frames; "
          f"frame 0 tem {len(f[0])} layers")
