import pefile, struct

pe = pefile.PE('OP2SHRES.DLL')

def parse_dlg(did):
    for res_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if res_type.id == 5:
            for r in res_type.directory.entries:
                if r.id == did:
                    sub = r.directory.entries[0]
                    offset = pe.get_offset_from_rva(sub.data.struct.OffsetToData)
                    size = sub.data.struct.Size
                    with open('OP2SHRES.DLL', 'rb') as f:
                        f.seek(offset)
                        data = f.read(size)
                    style, extstyle, cdit, x, y, cx, cy = struct.unpack('<IIHHHHH', data[:18])
                    pos = 18
                    # menu
                    m = struct.unpack('<H', data[pos:pos+2])[0]
                    pos += 2
                    if m == 0xFFFF: pos += 2
                    elif m != 0:
                        while data[pos:pos+2] != b'\x00\x00': pos += 2
                        pos += 2
                    # class
                    c = struct.unpack('<H', data[pos:pos+2])[0]
                    pos += 2
                    if c == 0xFFFF: pos += 2
                    elif c != 0:
                        while data[pos:pos+2] != b'\x00\x00': pos += 2
                        pos += 2
                    # title
                    tstart = pos
                    while data[pos:pos+2] != b'\x00\x00': pos += 2
                    title = data[tstart:pos].decode('utf-16le')
                    pos += 2
                    font = ''
                    if style & 0x40:
                        pt = struct.unpack('<H', data[pos:pos+2])[0]
                        pos += 2
                        fstart = pos
                        while data[pos:pos+2] != b'\x00\x00': pos += 2
                        font = data[fstart:pos].decode('utf-16le')
                        pos += 2
                    print(f"Dialog {did}: \"{title}\" ({cx}x{cy}) items={cdit} font=\"{font}\"")
                    
                    class_map = {0x0080: 'Button', 0x0081: 'Edit', 0x0082: 'Static', 0x0083: 'ListBox', 0x0084: 'ScrollBar', 0x0085: 'ComboBox'}
                    for idx in range(cdit):
                        if pos % 4 != 0: pos += 4 - (pos % 4)
                        istyle, iextstyle, ix, iy, icx, icy, iid = struct.unpack('<IIhhhhH', data[pos:pos+18])
                        pos += 18
                        ic = struct.unpack('<H', data[pos:pos+2])[0]
                        pos += 2
                        if ic == 0xFFFF:
                            cval = struct.unpack('<H', data[pos:pos+2])[0]
                            pos += 2
                            iclass = class_map.get(cval, f"Class_0x{cval:04X}")
                        elif ic != 0:
                            cs = pos - 2
                            while data[pos:pos+2] != b'\x00\x00': pos += 2
                            iclass = data[cs:pos].decode('utf-16le')
                            pos += 2
                        else: iclass = ''
                        
                        it = struct.unpack('<H', data[pos:pos+2])[0]
                        pos += 2
                        if it == 0xFFFF:
                            tval = struct.unpack('<H', data[pos:pos+2])[0]
                            pos += 2
                            itext = f"Res_0x{tval:04X}"
                        elif it != 0:
                            ts = pos - 2
                            while data[pos:pos+2] != b'\x00\x00': pos += 2
                            itext = data[ts:pos].decode('utf-16le')
                            pos += 2
                        else: itext = ''
                        
                        extra = struct.unpack('<H', data[pos:pos+2])[0]
                        pos += 2 + extra
                        print(f"  [{idx:2d}] ID {iid:5d}: {iclass:<10} \"{itext}\" @ ({ix}, {iy}, {icx}, {icy})")

parse_dlg(137)
parse_dlg(140)
parse_dlg(142)
