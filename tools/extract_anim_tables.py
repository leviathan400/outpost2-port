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

if __name__ == "__main__" and len(sys.argv) == 1:
    sys.exit(main())


# ---------------------------------------------------------------------------
# ESTRUTURAS.  Nao usam a tabela direcional +0x24C: tem DUAS tabelas de 10
# ESTADOS, preenchidas por um init em [ecx+...]:
#     +0x250 + estado*4   tabela A (flags & 0x1000)
#     +0x278 + estado*4   tabela B
# Resolvidas por sub_483720 (vt+0x90); estado = [unit+0x34] >> 16, escolhido
# por sub_408930 pelo dano: 0 intacto, 1 >=30%, 2 >=60%, 3 destruicao, 5+ ambiente.
# Estado 0 mostra o ULTIMO frame (a animacao de construcao terminada).
# O init vem colado num thunk `mov eax, offset aXXX ; "CODE_NAME"`.
# ---------------------------------------------------------------------------
def structures():
    lines = open(ASM, encoding="latin-1").read().split("\n")
    STR = re.compile(r'^mov\s+eax, offset a\w+\s*;\s*"([A-Z0-9_]+)"')
    MI  = re.compile(r'^mov\s+dword ptr \[ecx\+([0-9A-F]+)h\], ([0-9A-F]+)h?(?:\s*;.*)?$')
    RI  = re.compile(r'^mov\s+(e[a-z][a-z]), ([0-9A-F]+)h?(?:\s*;.*)?$')
    MR  = re.compile(r'^mov\s+\[ecx\+([0-9A-F]+)h\], (e[a-z][a-z])$')
    out = []; lastname = None; cur = None; f = {}; regs = {}
    for raw in lines:
        s = raw.strip()
        m = PROC.match(s)
        if m: cur = m.group(1); f = {}; regs = {}; continue
        if ENDP.match(s):
            if cur and (0x250 in f or 0x278 in f) and lastname:
                out.append((lastname, cur, dict(f)))
            cur = None; continue
        if cur is None: continue
        m = STR.match(s)
        if m: lastname = m.group(1); continue
        m = RI.match(s)
        if m:
            v = parse_imm(m.group(2))
            if v is not None: regs[m.group(1)] = v
            continue
        m = MI.match(s)
        if m:
            o, v = parse_imm(m.group(1)), parse_imm(m.group(2))
            if o is not None and v is not None: f[o] = v
            continue
        m = MR.match(s)
        if m:
            o = parse_imm(m.group(1))
            if o is not None and m.group(2) in regs: f[o] = regs[m.group(2)]
    return out

# ---------------------------------------------------------------------------
# map_id REAL.  O engine indexa os descritores por `off_4E1340[map_id]` (ex.
# CreateUnit, sub_43ACA0 `mov ecx, off_4E1340[eax*4]`).  A ordem das linhas das
# planilhas .txt NAO e o map_id.  Resolvemos nome -> map_id pela cadeia real:
#   estruturas: ctor estatico `mov D, offset V` ; V[1] = GetName
#   veiculos:   V[0] = fabrica (loc_ do switch de CreateUnit) -> `call C` (ctor da
#               unidade) -> C grava `[reg] = offset U` (vtable da unidade) ->
#               U[0] = getter `mov eax, offset D`
#   e D = off_4E1340[map_id].
# ---------------------------------------------------------------------------
def resolve_map_ids():
    lines = [l.rstrip("\n") for l in open(ASM, encoding="latin-1")]
    LBL = re.compile(r"^(\w+)\s+(?:proc near|dd offset (\w+))")
    STRN = re.compile(r'^mov\s+eax, offset a\w+\s*;\s*"([A-Z0-9_]+)"$')
    # 1) descritores por map_id
    i0 = next(i for i, l in enumerate(lines) if l.startswith("off_4E1340 "))
    descs = []
    for k in range(0x100):
        m = re.search(r"dd offset (dword_\w+)", lines[i0 + k])
        if not m: break
        descs.append(m.group(1))
    # 2) procs: corpo; nomes (GetName) e getters (mov eax, offset dword_X)
    body = {}; cur = None
    for l in lines:
        s = l.strip()
        m = PROC.match(s)
        if m: cur = m.group(1); body[cur] = []; continue
        if ENDP.match(s): cur = None; continue
        if cur: body[cur].append(s)
    getname = {}; getter = {}
    for p, b in body.items():
        code = [x for x in b if x and not x.startswith(";")]
        if len(code) == 2 and code[1] == "retn":
            m = STRN.match(code[0])
            if m: getname[p] = m.group(1)
            m = re.match(r"^mov\s+eax, offset (dword_\w+)$", code[0])
            if m: getter[p] = m.group(1)
    # 3) vtables: rotulo -> lista de entradas
    vt = {}; curv = None
    for l in lines:
        m = re.match(r"^(off_\w+)\s+dd offset (\w+)", l)
        if m: curv = m.group(1); vt[curv] = [m.group(2)]; continue
        m = re.match(r"^\s*dd offset (\w+)", l)
        if m and curv: vt[curv].append(m.group(1)); continue
        curv = None
    # 4) ctor de unidade: proc que grava `mov dword ptr [reg], offset U`
    ctor_vt = {}
    for p, b in body.items():
        for s in b:
            m = re.match(r"^mov\s+dword ptr \[e\w\w\], offset (off_\w+)$", s)
            if m: ctor_vt.setdefault(p, []).append(m.group(1))
    # 5) fabrica: bloco `loc_` ate o `retn`; vtables gravadas em ordem (pelos
    #    ctors chamados e/ou inline `mov dword ptr [esi], offset U`).
    fab = {}
    for i, l in enumerate(lines):
        m = re.match(r"^(loc_\w+):", l)
        if not m: continue
        seq = []
        for s2 in lines[i + 1:i + 40]:
            s2 = s2.strip()
            if s2.startswith("retn"): break
            mc = re.match(r"^call\s+(sub_\w+)$", s2)
            if mc and mc.group(1) in ctor_vt: seq += ctor_vt[mc.group(1)]
            mv = re.match(r"^mov\s+dword ptr \[e\w\w\], offset (off_\w+)$", s2)
            if mv: seq.append(mv.group(1))
        if seq: fab[m.group(1)] = seq
    # 6) ctor estatico das estruturas
    static_vt = {}
    for l in lines:
        m = re.match(r"^\s*mov\s+(dword_\w+), offset (off_\w+)$", l)
        if m: static_vt.setdefault(m.group(2), m.group(1))
    idx_of = {d: i for i, d in enumerate(descs)}
    name2id = {}
    for v, ents in vt.items():
        if len(ents) < 2 or ents[1] not in getname: continue
        name = getname[ents[1]]
        d = static_vt.get(v)
        if d is None and ents[0] in fab:
            # a BASE e gravada antes da DERIVADA: vale a ultima que tem getter
            for u in reversed(fab[ents[0]]):
                g = vt.get(u, [None])[0]
                if g in getter: d = getter[g]; break
        if d in idx_of:
            name2id.setdefault(name, idx_of[d])
    return name2id

if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "--mapids":
    for n, i in sorted(resolve_map_ids().items(), key=lambda x: x[1]):
        print(f"0x{i:02X} {n}")

if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "--structures":
    BIAS = 0x65
    for name, proc, f in structures():
        A = [f.get(0x250 + i*4, 0) for i in range(10)]
        B = [f.get(0x278 + i*4, 0) for i in range(10)]
        r = lambda t: [v - BIAS if v else -1 for v in t]
        print(f"{name:24} {proc}  A={r(A)}  B={r(B)}")


# ---------------------------------------------------------------------------
# --emit: gera src/game/vehicle_anim.cpp e src/game/structure_anim.cpp
# indexados pelo map_id REAL (resolve_map_ids).  Os campos vem do init de cada
# descritor = vt[4] da vtable do descritor (a mesma vtable cujo vt[1] e GetName).
# ---------------------------------------------------------------------------
def init_fields_by_name():
    lines = [l.rstrip("\n") for l in open(ASM, encoding="latin-1")]
    STRN = re.compile(r'^mov\s+eax, offset a\w+\s*;\s*"([A-Z0-9_]+)"$')
    MI = re.compile(r"^mov\s+dword ptr \[(e[a-z][a-z])\+([0-9A-F]+)h\], ([0-9A-F]+)h?(?:\s*;.*)?$")
    RI = re.compile(r"^mov\s+(e[a-z][a-z]), ([0-9A-F]+)h?(?:\s*;.*)?$")
    MR = re.compile(r"^mov\s+\[(e[a-z][a-z])\+([0-9A-F]+)h\], (e[a-z][a-z])$")
    # escritas de BYTE (ex. variante ambiente desc+0x2A6/0x2A7)
    MB = re.compile(r"^mov\s+byte ptr \[(e[a-z][a-z])\+([0-9A-F]+)h\], ([0-9A-F]+)h?(?:\s*;.*)?$")
    MR8 = re.compile(r"^mov\s+\[(e[a-z][a-z])\+([0-9A-F]+)h\], ([abcd])l$")
    body = {}; cur = None
    for l in lines:
        s = l.strip()
        m = PROC.match(s)
        if m: cur = m.group(1); body[cur] = []; continue
        if ENDP.match(s): cur = None; continue
        if cur: body[cur].append(s)
    def fields(p):
        f = {}; regs = {}
        for s in body.get(p, []):
            m = RI.match(s)
            if m:
                v = parse_imm(m.group(2))
                if v is not None: regs[m.group(1)] = v
                continue
            m = MI.match(s)
            if m:
                o, v = parse_imm(m.group(2)), parse_imm(m.group(3))
                if o is not None and v is not None: f[o] = v
                continue
            m = MR.match(s)
            if m:
                o = parse_imm(m.group(2))
                if o is not None and m.group(3) in regs: f[o] = regs[m.group(3)]
                continue
            m = MB.match(s)
            if m:
                o, v = parse_imm(m.group(2)), parse_imm(m.group(3))
                if o is not None and v is not None: f[o] = v & 0xFF
                continue
            m = MR8.match(s)
            if m:
                o = parse_imm(m.group(2)); r = "e" + m.group(3) + "x"
                if o is not None and r in regs: f[o] = regs[r] & 0xFF
        return f
    out = {}
    vt = {}; curv = None
    for l in lines:
        m = re.match(r"^(off_\w+)\s+dd offset (\w+)", l)
        if m: curv = m.group(1); vt[curv] = [m.group(2)]; continue
        m = re.match(r"^\s*dd offset (\w+)", l)
        if m and curv: vt[curv].append(m.group(1)); continue
        curv = None
    for v, ents in vt.items():
        if len(ents) < 5: continue
        code = [x for x in body.get(ents[1], []) if x and not x.startswith(";")]
        if len(code) != 2: continue
        m = STRN.match(code[0])
        if not m: continue
        out.setdefault(m.group(1), (ents[4], fields(ents[4])))
    return out

def emit():
    ids = resolve_map_ids()
    byid = {}
    for n, i in ids.items(): byid.setdefault(i, n)
    flds = init_fields_by_name()
    BIAS = 0x65
    # ---- veiculos (map_id 1..0x10), valor CRU (com vies) como o descritor
    rowsA, rowsB = [], []
    for t in range(0x11):
        n = byid.get(t, "none")
        p, f = flds.get(n, ("-", {}))
        A = [f.get(0x24C + i * 4, 0) for i in range(16)]
        B = [f.get(0x34C + i * 4, 0) for i in range(16)]
        lab = f"/* 0x{t:02X} {n:<12} {p:<10} */"
        fmt = lambda r: "{" + ", ".join(f"{v:4d}" for v in r) + " }" if any(r) else "{0}"
        rowsA.append(f"    {lab} {fmt(A)},")
        rowsB.append(f"    {lab} {fmt(B)},")
    vnames = "\n".join(f'    "{byid[t]}",  // 0x{t:02X}' if t in byid else f"    nullptr,  // 0x{t:02X}"
                       for t in range(0x11))
    veh = (VEH_TMPL.replace("@A@", "\n".join(rowsA)).replace("@B@", "\n".join(rowsB))
           .replace("@NAMES@", vnames))
    open("src/game/vehicle_anim.cpp", "w", encoding="utf-8", newline="\n").write(veh)
    # ---- estruturas (map_id 0x15..0x3A), SEM o vies
    rows = []
    for t in range(0x15, 0x3B):
        n = byid[t]; p, f = flds[n]
        A = [f.get(0x250 + i * 4, 0) for i in range(10)]
        B = [f.get(0x278 + i * 4, 0) for i in range(10)]
        r = lambda tb: "{ " + ", ".join(f"{(v - BIAS) if v else -1:4d}" for v in tb) + " }"
        rows.append(f"    /* 0x{t:02X} {n:<14} {p} */ {{ {r(A)},\n"
                    f"    {'':38}  {r(B)} }},")
    names = "\n".join(f'    "{byid[t]}",  // 0x{t:02X}' for t in range(0x15, 0x3B))
    amb = "\n".join(f"    {{ {flds[byid[t]][1].get(0x2A6, 0)}, {flds[byid[t]][1].get(0x2A7, 0)} }},"
                    f"  // 0x{t:02X} {byid[t]}" for t in range(0x15, 0x3B))
    st = (STR_TMPL.replace("@ROWS@", "\n".join(rows)).replace("@NAMES@", names)
          .replace("@AMB@", amb))
    open("src/game/structure_anim.cpp", "w", encoding="utf-8", newline="\n").write(st)
    print("[*] gerados src/game/vehicle_anim.cpp e src/game/structure_anim.cpp")

VEH_TMPL = """#include "vehicle_anim.h"

namespace op2 {

// Tabelas de animacao dos VEICULOS por map_id REAL (indice de off_4E1340, = enum
// do SDK), GERADAS por tools/extract_anim_tables.py --emit. NAO EDITAR a mao.
// Fonte: init do descritor (vt[4] da vtable cujo vt[1] e GetName):
//   +0x24C tabela A (16 direcoes) / +0x34C tabela B.
// Valores CRUS como no descritor (indiceReal + 0x65); 0 = sem tabela.
static const int kVehicleAnim[0x11][16] = {
@A@
};

// Tabela B (+0x34C).
static const int kVehicleAnimB[0x11][16] = {
@B@
};

// Vies entre o valor guardado no descritor/[unit+0x40] e o indice real no array
// de animacao (sub_403E00 faz `lea eax, [ecx-65h]`).
static const int kAnimIndexBias = 0x65;

bool hasVehicleAnim(int type) {
    return type >= 1 && type <= 0x10 && kVehicleAnim[type][0] != 0;
}

int vehicleAnimIndex(int type, int rotation, bool isEden) {
    if (!hasVehicleAnim(type)) return -1;
    int dir = ((rotation + 8) & 0xF0) >> 4;   // 16 direcoes (sub_44C5C0/sub_410E50)
    if (dir < 0 || dir > 15) dir = 0;
    int a = isEden ? kVehicleAnim[type][dir] : kVehicleAnimB[type][dir];
    if (!a) a = kVehicleAnim[type][dir];       // fallback p/ tabela A (sub_44C4F0)
    if (!a) return -1;
    int idx = a - kAnimIndexBias;
    return idx >= 0 ? idx : -1;
}

// Code_Name (GetName = vt[1] do descritor) por map_id 0x00..0x10.
static const char* const kVehicleCodeName[0x11] = {
@NAMES@
};

const char* vehicleCodeName(int type) {
    if (type < 0 || type > 0x10) return nullptr;
    return kVehicleCodeName[type];
}

} // namespace op2
"""

STR_TMPL = """#include "structure_anim.h"

namespace op2 {

// Tabelas de animacao das ESTRUTURAS por map_id REAL (0x15..0x3A, indice de
// off_4E1340 = enum do SDK), GERADAS por tools/extract_anim_tables.py --emit.
// NAO EDITAR a mao.
//
// Cada estrutura tem DUAS tabelas de 10 ESTADOS no descritor (nao 16 direcoes,
// como os veiculos):  +0x250 tabela A (flags & 0x1000)  /  +0x278 tabela B.
// Valores ja SEM o vies 0x65 (sub_403E00: `lea eax,[ecx-65h]`); -1 = nenhuma.
// Resolvido em sub_483720 (vt+0x90); estado = [unit+0x34] >> 16 (sub_4835A0).
static const int kStructAnim[38][2][10] = {
@ROWS@
};

int structureAnimIndex(int type, int state, bool tableA) {
    if (type < 0x15 || type > 0x3A || state < 0 || state > 9) return -1;
    return kStructAnim[type - 0x15][tableA ? 0 : 1][state];
}

// Code_Name (GetName = vt[1] do descritor) por map_id 0x15..0x3A. E por este
// nome que o parser do BUILDING.TXT (sub_445070) acha o descritor da linha.
static const char* const kStructCodeName[38] = {
@NAMES@
};

const char* structureCodeName(int type) {
    if (type < 0x15 || type > 0x3A) return nullptr;
    return kStructCodeName[type - 0x15];
}

// Variante da animacao ambiente: byte desc+0x2A6 (tabela A) / +0x2A7 (B) =
// coluna 1..4 da tabela byte_4E4138 lida por sub_4835C0; 0 = sem ambiente.
static const int kStructAmbientVariant[38][2] = {
@AMB@
};

int structureAmbientVariant(int type, bool tableA) {
    if (type < 0x15 || type > 0x3A) return 0;
    return kStructAmbientVariant[type - 0x15][tableA ? 0 : 1];
}

} // namespace op2
"""

if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "--emit":
    emit()
