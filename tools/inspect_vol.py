import struct

def parse_vol(path):
    with open(path, 'rb') as f:
        magic = f.read(4)
        if magic != b'VOL ':
            print(f"Not a VOL file: {magic}")
            return
        
        # Read chunks
        files = []
        f.seek(0)
        data = f.read()
        
        # Search for 'vols'
        pos = data.find(b'vols')
        if pos != -1:
            str_size = struct.unpack('<I', data[pos+4:pos+8])[0]
            str_table = data[pos+8:pos+8+str_size]
            names = [s.decode('latin-1', errors='replace') for s in str_table.split(b'\x00') if s]
            print(f"{path}: Found {len(names)} files in vols string table. First 25:")
            for name in names[:25]:
                print(f"  {name}")

parse_vol('SOUND.VOL')
