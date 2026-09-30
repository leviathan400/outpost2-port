#!/usr/bin/env python3
import pefile, struct, json, os

def parse_dialog(data):
    style, extstyle, cdit, x, y, cx, cy = struct.unpack('<I I H h h h h', data[:18])
    pos = 18
    # menu
    m = struct.unpack('<H', data[pos:pos+2])[0]
    pos += 2
    if m == 0xFFFF:
        pos += 2
    elif m != 0:
        while data[pos:pos+2] != b'\x00\x00':
            pos += 2
        pos += 2
    # window class
    c = struct.unpack('<H', data[pos:pos+2])[0]
    pos += 2
    if c == 0xFFFF:
        pos += 2
    elif c != 0:
        while data[pos:pos+2] != b'\x00\x00':
            pos += 2
        pos += 2
    # title
    title_start = pos
    while data[pos:pos+2] != b'\x00\x00':
        pos += 2
    title = data[title_start:pos].decode('utf-16le', errors='replace')
    pos += 2
    font_name = ''
    font_size = 0
    if style & 0x40:
        font_size = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        fstart = pos
        while data[pos:pos+2] != b'\x00\x00':
            pos += 2
        font_name = data[fstart:pos].decode('utf-16le', errors='replace')
        pos += 2
    controls = []
    class_map = {0x0080: 'Button', 0x0081: 'Edit', 0x0082: 'Static', 0x0083: 'ListBox', 0x0084: 'ScrollBar', 0x0085: 'ComboBox'}
    for _ in range(cdit):
        if pos % 4 != 0:
            pos += 4 - (pos % 4)
        istyle, iextstyle, ix, iy, icx, icy, iid = struct.unpack('<I I h h h h H', data[pos:pos+18])
        pos += 18
        ic = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        if ic == 0xFFFF:
            cval = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
            klass = class_map.get(cval, f'Class_0x{cval:04X}')
        elif ic != 0:
            cstart = pos - 2
            while data[pos:pos+2] != b'\x00\x00':
                pos += 2
            klass = data[cstart:pos].decode('utf-16le', errors='replace')
            pos += 2
        else:
            klass = ''
        it = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        if it == 0xFFFF:
            pos += 2
            text = ''
        elif it != 0:
            tstart = pos - 2
            while data[pos:pos+2] != b'\x00\x00':
                pos += 2
            text = data[tstart:pos].decode('utf-16le', errors='replace')
            pos += 2
        else:
            text = ''
        extra = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2 + extra
        controls.append({
            'id': iid,
            'class': klass,
            'text': text,
            'x': ix,
            'y': iy,
            'cx': icx,
            'cy': icy,
            'style': istyle
        })
    return {
        'style': style,
        'extStyle': extstyle,
        'cdit': cdit,
        'x': x,
        'y': y,
        'cx': cx,
        'cy': cy,
        'title': title,
        'fontName': font_name,
        'fontSize': font_size,
        'controls': controls
    }

def main():
    dll_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'OP2SHRES.DLL'))
    pe = pefile.PE(dll_path)
    for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if entry.id != 5:
            continue
        for res in entry.directory.entries:
            if res.id == 152:  # Main menu dialog
                data_rva = res.directory.entries[0].data.struct.OffsetToData
                size = res.directory.entries[0].data.struct.Size
                offset = pe.get_offset_from_rva(data_rva)
                ddata = pe.__data__[offset:offset+size]
                dialog = parse_dialog(ddata)
                out_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'menu_dialog.json'))
                with open(out_path, 'w', encoding='utf-8') as f:
                    json.dump(dialog, f, indent=2)
                print(f'Wrote menu dialog JSON to {out_path}')
                return
    print('Main menu dialog (ID 152) not found.')

if __name__ == '__main__':
    main()
