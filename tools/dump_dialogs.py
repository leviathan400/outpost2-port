import pefile
import struct

pe = pefile.PE('OP2SHRES.DLL')

def parse_dialog(data):
    style, extstyle, cdit, x, y, cx, cy = struct.unpack('<IIHHHHH', data[:18])
    pos = 18
    # menu
    m = struct.unpack('<H', data[pos:pos+2])[0]
    pos += 2
    if m == 0xFFFF:
        pos += 2
    elif m != 0:
        while data[pos:pos+2] != b'\x00\x00': pos += 2
        pos += 2
    # window class
    c = struct.unpack('<H', data[pos:pos+2])[0]
    pos += 2
    if c == 0xFFFF:
        pos += 2
    elif c != 0:
        while data[pos:pos+2] != b'\x00\x00': pos += 2
        pos += 2
    # title
    title_start = pos
    while data[pos:pos+2] != b'\x00\x00': pos += 2
    title = data[title_start:pos].decode('utf-16le', errors='replace')
    pos += 2
    font = ''
    if style & 0x40: # DS_SETFONT
        pt = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        fstart = pos
        while data[pos:pos+2] != b'\x00\x00': pos += 2
        font = f'{data[fstart:pos].decode("utf-16le", errors="replace")} {pt}pt'
        pos += 2

    # parse items
    items = []
    class_map = {0x0080: 'Button', 0x0081: 'Edit', 0x0082: 'Static', 0x0083: 'ListBox', 0x0084: 'ScrollBar', 0x0085: 'ComboBox'}
    for _ in range(cdit):
        # align to DWORD
        if pos % 4 != 0:
            pos += 4 - (pos % 4)
        istyle, iextstyle, ix, iy, icx, icy, iid = struct.unpack('<IIhhhhH', data[pos:pos+18])
        pos += 18
        # item class
        ic = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        if ic == 0xFFFF:
            cval = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
            iclass = class_map.get(cval, f'Class_0x{cval:04X}')
        elif ic != 0:
            cstart = pos - 2
            while data[pos:pos+2] != b'\x00\x00': pos += 2
            iclass = data[cstart:pos].decode('utf-16le', errors='replace')
            pos += 2
        else:
            iclass = ''

        # item text
        it = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        if it == 0xFFFF:
            tval = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
            itext = f'Res_0x{tval:04X}'
        elif it != 0:
            tstart = pos - 2
            while data[pos:pos+2] != b'\x00\x00': pos += 2
            itext = data[tstart:pos].decode('utf-16le', errors='replace')
            pos += 2
        else:
            itext = ''

        # extra bytes
        extra = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2 + extra
        items.append((iid, iclass, itext, ix, iy, icx, icy, istyle))

    return style, cdit, x, y, cx, cy, title, font, items

for res_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if res_type.id == 5: # RT_DIALOG
        for res_id in res_type.directory.entries:
            sub = res_id.directory.entries[0]
            offset = pe.get_offset_from_rva(sub.data.struct.OffsetToData)
            size = sub.data.struct.Size
            with open('OP2SHRES.DLL', 'rb') as f:
                f.seek(offset)
                ddata = f.read(size)
                try:
                    style, cdit, x, y, cx, cy, title, font, items = parse_dialog(ddata)
                    print(f'=== Dialog ID {res_id.id}: "{title}" ({cx}x{cy}) controls={len(items)} ===')
                    for it in items:
                        print(f'    ID {it[0]:4d}: {it[1]:<10} "{it[2]}" @ ({it[3]}, {it[4]}, {it[5]}, {it[6]}) style=0x{it[7]:08X}')
                except Exception as e:
                    print(f'Dialog {res_id.id}: parse error {e}')
