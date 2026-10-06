"""Extrai as tabelas de animacao dos DESCRITORES de unidade/estrutura do OP2.

O descritor guarda duas tabelas de 16 direcoes:
    +0x24C .. +0x288  tabela A  (faccao via [unit+0x44] & 0x1000)
    +0x34C .. +0x388  tabela B
O valor guardado e (indiceReal + 0x65) -- sub_403E00 faz `lea eax,[ecx-65h]`.
Resolvidas em sub_44C4F0 (vtable+0x80) por `desc[0x24C + (state*16+dir)*4]`.

As funcoes de init (vt[4]) preenchem os campos com imediatos, em duas formas:
    mov  dword ptr [esi+24Ch], 184h        <- imediato direto
    mov  eax, 65h / mov [esi+24Ch], eax    <- imediato cacheado em registrador
Este script simula os dois.  NAO inventa nada: so le o disassembly.
"""
import re, sys, collections

ASM = "Outpost2_src/OUTPOST2.asm"
PROC = re.compile(r"^(sub_[0-9A-F]+) proc near")
ENDP = re.compile(r"^(sub_[0-9A-F]+) endp")
MOV_IMM_MEM = re.compile(r"^mov\s+dword ptr \[(e[a-z][a-z])\+([0-9A-F]+)h\], ([0-9A-F]+)h?(?:\s*;.*)?$")
MOV_REG_IMM = re.compile(r"^mov\s+(e[a-z][a-z]), ([0-9A-F]+)h?(?:\s*;.*)?$")
MOV_MEM_REG = re.compile(r"^mov\s+\[(e[a-z][a-z])\+([0-9A-F]+)h\], (e[a-z][a-z])$")
MOV_OFF = re.compile(r"^mov\s+eax, offset (\w+)")

def parse_imm(s):
    try:
        return int(s, 16)
    except ValueError:
        return None

def main():
    procs = {}          # nome -> {offset: valor}
    thunks = {}         # nome -> global retornado (mov eax, offset X / retn)
    order = []
    cur = None; fields = {}; regs = {}; body = []
    for raw in open(ASM, encoding="latin-1"):
        line = raw.rstrip("\n").strip()
        m = PROC.match(line)
        if m:
            cur = m.group(1); fields = {}; regs = {}; body = []
            order.append(cur)
            continue
        if ENDP.match(line):
            if cur:
                if fields: procs[cur] = fields
                # thunk: corpo curto com "mov eax, offset X"
                if len(body) <= 3:
                    for b in body:
                        mo = MOV_OFF.match(b)
                        if mo: thunks[cur] = mo.group(1)
            cur = None
            continue
        if cur is None: continue
        body.append(line)
        m = MOV_REG_IMM.match(line)
        if m:
            v = parse_imm(m.group(2))
            if v is not None: regs[m.group(1)] = v
            continue
        m = MOV_IMM_MEM.match(line)
        if m:
            off = parse_imm(m.group(2)); val = parse_imm(m.group(3))
            if off is not None and val is not None: fields[off] = val
            continue
        m = MOV_MEM_REG.match(line)
        if m:
            off = parse_imm(m.group(2)); src = m.group(3)
            if off is not None and src in regs: fields[off] = regs[src]
            continue

    # descritores = procs que escrevem +0x24C
    BIAS = 0x65
    found = [(n, f) for n, f in procs.items() if 0x24C in f]
    print(f"[*] {len(found)} funcoes de init escrevem +0x24C\n")
    for name, f in sorted(found, key=lambda x: x[0]):
        A = [f.get(0x24C + i * 4) for i in range(16)]
        B = [f.get(0x34C + i * 4) for i in range(16)]
        nA = sum(1 for v in A if v is not None)
        nB = sum(1 for v in B if v is not None)
        def fmt(t):
            return [(v - BIAS if v else 0) for v in t[:16] if v is not None]
        print(f"{name}  A[{nA}] B[{nB}]  +0x1F0={f.get(0x1F0)}")
        print(f"   A(real) = {fmt(A)}")
        if nB: print(f"   B(real) = {fmt(B)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
