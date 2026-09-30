import pefile, struct

pe = pefile.PE('OP2SHRES.DLL')

for res_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if res_type.id == 5:
        for r in res_type.directory.entries:
            sub = r.directory.entries[0]
            offset = pe.get_offset_from_rva(sub.data.struct.OffsetToData)
            size = sub.data.struct.Size
            with open('OP2SHRES.DLL', 'rb') as f:
                f.seek(offset)
                raw = f.read(min(size, 64))
                ver = struct.unpack('<H', raw[:2])[0]
                sig = struct.unpack('<H', raw[2:4])[0]
                is_ex = (ver == 1 and sig == 0xFFFF)
                print(f"Dialog {r.id}: size={size}, ver={ver}, sig=0x{sig:04X}, is_ex={is_ex}")
