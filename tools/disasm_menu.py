import pefile, capstone
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

pe = pefile.PE('OP2SHELL.DLL')
cs = Cs(CS_ARCH_X86, CS_MODE_32)
data = open('OP2SHELL.DLL', 'rb').read()

rva = 0x5000
va = pe.OPTIONAL_HEADER.ImageBase + rva
code = data[rva-0x200:rva+0x300]
print(f"Disassembly around 0x13005000 (RVA 0x{rva:X}):")
for ins in cs.disasm(code, va-0x200):
    if ins.address >= 0x13004E80 and ins.address <= 0x13005200:
        print(f"  0x{ins.address:X}: {ins.mnemonic:<8} {ins.op_str}")
