import pefile, struct

pe = pefile.PE('OP2SHRES.DLL')

def extract_bmp(res_id, out_name):
    for res_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if res_type.id == 2:
            for r in res_type.directory.entries:
                if r.id == res_id:
                    sub = r.directory.entries[0]
                    offset = pe.get_offset_from_rva(sub.data.struct.OffsetToData)
                    size = sub.data.struct.Size
                    with open('OP2SHRES.DLL', 'rb') as f:
                        f.seek(offset)
                        dib_data = f.read(size)
                    
                    # DIB has BITMAPINFOHEADER + Palette + Bits
                    # To make a valid BMP file, prepend BITMAPFILEHEADER (14 bytes)
                    # bfType = 'BM', bfSize = 14 + size, bfReserved1=0, bfReserved2=0
                    # bfOffBits: header_size + palette_size
                    hdr_size = struct.unpack('<I', dib_data[:4])[0]
                    bpp = struct.unpack('<H', dib_data[14:16])[0]
                    clr_used = struct.unpack('<I', dib_data[32:36])[0]
                    if clr_used == 0 and bpp <= 8:
                        clr_used = 1 << bpp
                    palette_size = clr_used * 4
                    off_bits = 14 + hdr_size + palette_size
                    bf_size = 14 + size
                    file_header = struct.pack('<2sIHHI', b'BM', bf_size, 0, 0, off_bits)
                    
                    with open(out_name, 'wb') as out:
                        out.write(file_header)
                        out.write(dib_data)
                    print(f"Extracted {out_name}: {size} bytes, off_bits={off_bits}")

extract_bmp(225, 'tools/menu_bg_225.bmp')
extract_bmp(190, 'tools/menu_bg_190.bmp')
extract_bmp(145, 'tools/bmp_145.bmp')
extract_bmp(220, 'tools/bmp_220.bmp')
