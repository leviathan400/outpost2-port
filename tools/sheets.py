#!/usr/bin/env python3
"""
Parser das planilhas de dados do Outpost 2 (SHEETS.VOL, comprimidas em LZHUF).

Dois formatos:

1) Tabelas TSV (building, vehicles, weapons, mines, morale, space):
   duas linhas de cabeçalho (nomes de coluna quebrados em 2 linhas) + linhas de
   dados separadas por TAB. A 1ª coluna é o Code_Name (ex.: LASER).

2) Árvores de tecnologia (edentek, ply_tek, multitek, tutortek):
   blocos  BEGIN_TECH "Nome" <id> ... <props> ... END_TECH
   com propriedades CATEGORY, REQUIRES, COST, DESCRIPTION, TEASER,
   MAX_SCIENTISTS, LAB, UNIT_PROP, EDEN_COST, PLYMOUTH_COST, etc.
   Linhas iniciadas por ';' são comentários.

O bloco descomprimido pode ter alguns bytes de "lixo" no fim (padding do LZHUF);
cortamos na primeira linha com caractere não-imprimível.

Mecanismo confirmado no disassembly (sub_411220 = leitor de linha; sub_4112F0 =
próximo registro; loader de BUILDING.TXT em 0x445xxx):
  * o jogo lê linha a linha (buffer 512B) e PARA em 0x1A (Ctrl-Z) ou no fim do
    stream — clean_lines() reproduz isso (0x1A < 32 já corta);
  * registros são acessados POR NOME (Code_Name), não por contagem de linhas —
    por isso o lixo final é inofensivo (nenhum nome real casa com ele);
  * campos são separados por TAB (delimitador em [edi+14h]); campos vazios contam;
  * a árvore de tecnologia usa keywords BEGIN_TECH/END_TECH com comparação
    case-insensitive (__strcmpi no código).
"""
import re
import sys

TABLE_FILES = ["building.txt", "vehicles.txt", "weapons.txt",
               "mines.txt", "morale.txt", "space.txt"]
TECH_FILES = ["edentek.txt", "ply_tek.txt", "multitek.txt", "tutortek.txt"]


def _decode(name, vol_path="SHEETS.VOL"):
    sys.path.insert(0, "tools")
    from vol import VolFile
    from lzhuf import LzhufDecoder
    v = VolFile(vol_path)
    e = [x for x in v.entries if x.name == name][0]
    return LzhufDecoder().decode(v.raw_block(e)).decode("latin-1")


def clean_lines(text):
    """Linhas até o início do lixo de padding (1ª linha não-imprimível)."""
    out = []
    for line in text.replace("\r\n", "\n").split("\n"):
        if all(32 <= ord(c) < 127 or c == "\t" for c in line):
            out.append(line)
        else:
            break
    return out


def parse_table(name):
    """Retorna (headers, rows). Detecta 1 ou 2 linhas de cabeçalho e valida as
    linhas de dados pela contagem de colunas (descarta o lixo de padding)."""
    lines = clean_lines(_decode(name))
    ncols = len(lines[0].split("\t"))

    # 2ª linha é continuação de cabeçalho se começar com TAB (1ª coluna vazia)
    h1 = lines[0].split("\t")
    has_h2 = len(lines) > 1 and lines[1].startswith("\t")
    h2 = lines[1].split("\t") if has_h2 else []
    headers = []
    for i in range(ncols):
        a = h1[i].strip() if i < len(h1) else ""
        b = h2[i].strip() if i < len(h2) else ""
        headers.append((a + " " + b).strip())

    rows = []
    for line in lines[(2 if has_h2 else 1):]:
        cols = [c.strip() for c in line.split("\t")]
        # linha válida: 1ª coluna "code-like" e pelo menos 2 colunas.
        # (rejeita linhas em branco e o lixo de padding, ex.: uma coluna 'tttt')
        if len(cols) < 2 or not re.match(r'^[A-Za-z0-9_]+$', cols[0]):
            continue
        # mapeia pelo cabeçalho quando a contagem bate; senão, colunas genéricas
        if len(cols) == ncols:
            rows.append({headers[i]: cols[i] for i in range(ncols)})
        else:
            rows.append({"Code": cols[0],
                         "values": [c for c in cols[1:] if c != ""]})
    return headers, rows


def parse_tech(name):
    """Retorna lista de techs: dict com name, id e propriedades."""
    lines = clean_lines(_decode(name))
    techs = []
    cur = None
    for line in lines:
        s = line.strip()
        if s.startswith(";") or not s:
            continue
        m = re.match(r'^BEGIN_TECH\s+"([^"]*)"\s+(\d+)', s)
        if m:
            cur = {"name": m.group(1), "id": int(m.group(2)), "props": {}}
            continue
        if s.startswith("END_TECH"):
            if cur:
                techs.append(cur)
                cur = None
            continue
        if cur is not None:
            parts = s.split(None, 1)
            key = parts[0]
            val = parts[1].strip() if len(parts) > 1 else ""
            # chaves repetidas (ex.: UNIT_PROP) viram lista
            if key in cur["props"]:
                if not isinstance(cur["props"][key], list):
                    cur["props"][key] = [cur["props"][key]]
                cur["props"][key].append(val)
            else:
                cur["props"][key] = val
    return techs


if __name__ == "__main__":
    print("=== TABELAS ===")
    for n in TABLE_FILES:
        headers, rows = parse_table(n)
        print(f"{n:14} {len(rows):3} registros, {len(headers)} colunas")
    print("\n=== ÁRVORES DE TECNOLOGIA ===")
    total = 0
    for n in TECH_FILES:
        techs = parse_tech(n)
        total += len(techs)
        print(f"{n:14} {len(techs):3} techs")
    print(f"total techs: {total}")
