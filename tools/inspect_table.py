import pefile
import struct

pe = pefile.PE('OP2SHELL.DLL')
ib = pe.OPTIONAL_HEADER.ImageBase

def get_str(va):
    if va == 0: return "NULL"
    rva = va - ib
    if rva < 0 or rva >= len(pe.get_memory_mapped_image()):
        return f"0x{va:X}"
    data = pe.get_memory_mapped_image()[rva:rva+128]
    return repr(data.split(b'\x00')[0].decode('latin-1', errors='replace'))

rva = 0x13012350 - ib
data = pe.get_memory_mapped_image()[rva:rva+300]
print(f"Table around 0x13012350:")
for offset in range(0, len(data)-4, 4):
    val = struct.unpack('<I', data[offset:offset+4])[0]
    va = 0x13012350 + offset
    s = get_str(val) if (0x13000000 <= val <= 0x13020000) else ""
    print(f"  0x{va:X} (+{offset:03X}): 0x{val:08X} ({val:10d}) {s}")
