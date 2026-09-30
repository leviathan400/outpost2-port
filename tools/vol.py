#!/usr/bin/env python3
"""
Parser do formato de container .VOL do Outpost 2 (Sierra/Dynamix, 1997).

O .VOL é o arquivo-arquivo (archive) do jogo, conceitualmente igual ao .WAD do
Doom/Quake: um diretório de entradas + os dados concatenados. A diferença é que
o VOL usa chunks com tag (estilo RIFF/IFF) e suporta compressão por arquivo.

Layout do arquivo
-----------------
  "VOL " <u32 len>            # container do diretório (payload = volh+vols+voli)
      "volh" <u32 len=0>      # volume header (vazio)
      "vols" <u32 len>        # STRING TABLE
          <u32 strTableLen>   #   tamanho real dos nomes
          char[]              #   nomes de arquivo, separados por \0  (+ padding)
      "voli" <u32 len>        # INDEX  (len/14 entradas)
          IndexEntry[14] ...  #   ver abaixo
  "VBLK" <u32 len> data ...   # um bloco por arquivo (dados; alinhado a 2 bytes)
  "VBLK" <u32 len> data ...
  ...

Observações de bits
-------------------
* Todo campo de tamanho de chunk tem o bit 0x80000000 como flag ("em uso").
  O tamanho real é (len & 0x7FFFFFFF).
* IndexEntry (14 bytes):
      u32 fileNameOffset   # offset dentro da string table; 0xFFFFFFFF = slot livre
      u32 dataBlockOffset  # offset absoluto no arquivo, aponta para o "VBLK"
      u32 fileSize         # tamanho ARMAZENADO (comprimido) do bloco de dados
      u8  compressionType  # 0=cru, 1=RLE, 2=LZ, 3=LZHUF  (byte em +0x0C)
      u8  flag             # 0x01 = em uso
* Compressão (confirmada pelo disassembly, sub_4A0E50 + sub_433F80):
  o tipo vem do byte em +0x0C da entrada. Os .txt de SHEETS.VOL usam tipo 3 =
  LZHUF (LZSS + Huffman adaptativo, Okumura 1989), com ring buffer N=4096 e
  lookahead F=60 (o construtor preenche 4096-60=0xFC4 posições com ' ').
  A descompressão ainda não está implementada aqui (ver PORT_STATUS.md).
"""
import struct
import os
import sys

USED_FLAG = 0x80000000
LEN_MASK = 0x7FFFFFFF
FREE = 0xFFFFFFFF
ENTRY_SIZE = 14


COMP_NAMES = {0: "cru", 1: "RLE", 2: "LZ", 3: "LZHUF"}


class VolEntry:
    __slots__ = ("name", "name_off", "data_off", "size", "comp", "flag",
                 "vblk_len")

    def __init__(self, name, name_off, data_off, size, comp, flag, vblk_len):
        self.name = name
        self.name_off = name_off
        self.data_off = data_off      # offset do "VBLK" no arquivo
        self.size = size              # tamanho armazenado (comprimido) do bloco
        self.comp = comp              # tipo de compressão (byte +0x0C)
        self.flag = flag              # byte +0x0D (0x01 = em uso)
        self.vblk_len = vblk_len      # tamanho do payload do VBLK

    @property
    def compressed(self):
        return self.comp != 0

    @property
    def comp_name(self):
        return COMP_NAMES.get(self.comp, f"?{self.comp}")


class VolFile:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            self.data = f.read()
        self.entries = []
        self._parse()

    def _u32(self, off):
        return struct.unpack_from("<I", self.data, off)[0]

    def _u16(self, off):
        return struct.unpack_from("<H", self.data, off)[0]

    def _parse(self):
        d = self.data
        if d[:4] != b"VOL ":
            raise ValueError(f"{self.path}: assinatura inválida {d[:4]!r}")

        # Diretório vive no payload do chunk "VOL " (começa em 8).
        # Localiza os sub-chunks vols e voli dentro do diretório.
        vols = d.find(b"vols", 0, 4096)
        voli = d.find(b"voli", 0, 4096)
        if vols < 0 or voli < 0:
            raise ValueError(f"{self.path}: chunks vols/voli não encontrados")

        # --- string table ---
        str_len = self._u32(vols + 8)
        self.str_base = vols + 12
        self.str_blob = d[self.str_base:self.str_base + str_len]

        # --- index ---
        idx_len = self._u32(voli + 4) & LEN_MASK
        idx_base = voli + 8
        count = idx_len // ENTRY_SIZE

        for i in range(count):
            e = idx_base + i * ENTRY_SIZE
            name_off = self._u32(e)
            if name_off == FREE:
                continue  # slot livre
            data_off = self._u32(e + 4)
            size = self._u32(e + 8)
            comp = d[e + 12]
            flag = d[e + 13]
            name = self._name_at(name_off)
            # payload do VBLK
            vblk_len = -1
            if d[data_off:data_off + 4] == b"VBLK":
                vblk_len = self._u32(data_off + 4) & LEN_MASK
            self.entries.append(
                VolEntry(name, name_off, data_off, size, comp, flag, vblk_len))

    def _name_at(self, off):
        end = self.data.find(b"\x00", self.str_base + off)
        return self.data[self.str_base + off:end].decode("latin-1")

    def raw_block(self, entry):
        """Bytes do payload do VBLK (ainda comprimidos, se for o caso)."""
        start = entry.data_off + 8  # pula tag "VBLK" + u32 len
        return self.data[start:start + entry.size]

    def read(self, entry):
        """Retorna os bytes descomprimidos do arquivo."""
        raw = self.raw_block(entry)
        if entry.comp == 0:
            return raw
        if entry.comp == 3:  # LZHUF
            from lzhuf import decompress
            return decompress(raw)
        raise NotImplementedError(
            f"{entry.name}: compressão {entry.comp_name} ainda não implementada")

    def extract_all(self, out_dir):
        os.makedirs(out_dir, exist_ok=True)
        ok = 0
        for e in self.entries:
            try:
                blob = self.read(e)
                with open(os.path.join(out_dir, e.name), "wb") as f:
                    f.write(blob)
                ok += 1
            except NotImplementedError as ex:
                print(f"  [pulado] {ex}")
        return ok


def summarize(path):
    v = VolFile(path)
    from collections import Counter
    types = Counter(e.comp_name for e in v.entries)
    print(f"\n{os.path.basename(path)}: {len(v.entries)} arquivos  {dict(types)}")
    for e in v.entries[:12]:
        print(f"  {e.comp_name:6} {e.size:9} {e.name}")
    if len(v.entries) > 12:
        print(f"  ... (+{len(v.entries) - 12})")
    return v


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        # modo scan: resume todos os .VOL da pasta atual
        for p in sorted(f for f in os.listdir(".") if f.upper().endswith(".VOL")):
            summarize(p)
    elif args[0] == "extract" and len(args) >= 2:
        v = VolFile(args[1])
        out = args[2] if len(args) >= 3 else args[1] + "_extracted"
        n = v.extract_all(out)
        print(f"Extraídos {n}/{len(v.entries)} arquivos para {out}/")
    else:
        summarize(args[0])
