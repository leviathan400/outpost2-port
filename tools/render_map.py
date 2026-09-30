#!/usr/bin/env python3
"""
Renderiza um mapa .map do Outpost 2 com os gráficos reais dos tilesets (PBMP),
juntando toda a pipeline: .VOL -> .map (grid de tiles) + tilesets well####.bmp.

Uso:  python tools/render_map.py eden01.map [saida.png] [--scale 1]

O tileIndex de cada célula (bits 5-31) indexa o POOL GLOBAL de tiles, formado
concatenando, na ordem listada no mapa, os tiles de cada tileset (32x32).
"""
import sys

sys.path.insert(0, "tools")
from vol import VolFile
from pbmp import Pbmp
from op2map import Op2Map


def render(map_name, vol_path="MAPS.VOL"):
    from PIL import Image
    v = VolFile(vol_path)
    idx = {e.name: e for e in v.entries}
    m = Op2Map(v.read(idx[map_name]))

    # carrega tilesets (o mapa lista nomes sem extensão) e monta o pool global
    def ts_name(n):
        return n if n.lower().endswith(".bmp") else n + ".bmp"

    pool = []  # global index -> (pbmp, local index)
    for name, _ in m.tilesets:
        ts = Pbmp(v.read(idx[ts_name(name)]))
        for i in range(ts.num_tiles):
            pool.append((ts, i))

    # cache só dos tiles usados (usa o layout real de faixas via m.cell)
    W, H = m.width, m.height
    used = {m.tile_index(m.cell(x, y)) for y in range(H) for x in range(W)}
    cache = {g: pool[g][0].tile_image(pool[g][1]) for g in used if g < len(pool)}

    out = Image.new("RGB", (W * 32, H * 32))
    for y in range(H):
        for x in range(W):
            im = cache.get(m.tile_index(m.cell(x, y)))
            if im:
                out.paste(im, (x * 32, y * 32))
    return m, out


if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "eden01.map"
    out_path = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("-") \
        else name.replace(".map", "") + "_terrain.png"
    scale = 1
    if "--scale" in sys.argv:
        scale = int(sys.argv[sys.argv.index("--scale") + 1])
    m, img = render(name)
    if scale != 1:
        from PIL import Image
        img = img.resize((img.width // scale, img.height // scale), Image.LANCZOS)
    img.save(out_path)
    print(f"{name}: {m.width}x{m.height} tiles -> {out_path} ({img.width}x{img.height})")
