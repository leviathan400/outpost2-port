import pefile, struct

pe = pefile.PE('OP2SHRES.DLL')

def inspect_bmp(res_id):
    for res_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if res_type.id == 2:
            for r in res_type.directory.entries:
                if r.id == res_id:
                    sub = r.directory.entries[0]
                    offset = pe.get_offset_from_rva(sub.data.struct.OffsetToData)
                    size = sub.data.struct.Size
                    with open('OP2SHRES.DLL', 'rb') as f:
                        f.seek(offset)
                        data = f.read(40)
                        sz, w, h, pl, bpp, comp = struct.unpack('<IiiHHI', data[:20])
                        print(f"Bitmap {res_id:3d}: {w}x{h} bpp={bpp} size={size}")

for id_val in [145, 190, 220, 225, 235, 236, 400, 401, 402, 403]:
    inspect_bmp(id_val)
