import pefile
import struct
import capstone
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

pe = pefile.PE('OP2SHELL.DLL')
cs = Cs(CS_ARCH_X86, CS_MODE_32)
data = open('OP2SHELL.DLL', 'rb').read()

# Let's search for 225 (0xE1), 190 (0xBE) or other resource IDs pushed in code
print("Searching for pushes of bitmap 225 (0xE1) or dialog IDs...")
for i in range(len(data)-5):
    # push 225: 68 E1 00 00 00
    if data[i:i+5] == b'\x68\xE1\x00\x00\x00':
        rva = pe.get_rva_from_offset(i)
        va = pe.OPTIONAL_HEADER.ImageBase + rva
        print(f"push 225 found at file offset 0x{i:X}, VA 0x{va:X}")
        # Disassemble 50 bytes around here
        code = data[i-10:i+40]
        for ins in cs.disasm(code, va-10):
            print(f"  0x{ins.address:X}: {ins.mnemonic:<8} {ins.op_str}")

# Search for "MAIN MENU" reference
main_menu_offset = data.find(b'MAIN MENU')
if main_menu_offset != -1:
    mm_rva = pe.get_rva_from_offset(main_menu_offset)
    mm_va = pe.OPTIONAL_HEADER.ImageBase + mm_rva
    print(f"\n'MAIN MENU' string at VA 0x{mm_va:X}")
    # Search for references to mm_va
    mm_bytes = struct.pack('<I', mm_va)
    for i in range(len(data)-4):
        if data[i:i+4] == mm_bytes:
            rva = pe.get_rva_from_offset(i)
            va = pe.OPTIONAL_HEADER.ImageBase + rva
            print(f"Reference to 'MAIN MENU' found at VA 0x{va:X}")
            code = data[i-20:i+60]
            for ins in cs.disasm(code, va-20):
                print(f"  0x{ins.address:X}: {ins.mnemonic:<8} {ins.op_str}")
