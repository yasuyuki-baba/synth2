"""Generate self-authored fixtures with: python3 generate.py /path/to/7zz

Test execution needs neither this script nor the external encoder.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parent
encoder = str(Path(sys.argv[1]).resolve())


def archive(name, source, *options):
    destination = root / (name + '.7z')
    destination.unlink(missing_ok=True)
    subprocess.run([encoder, 'a', '-bd', '-bso0', '-bsp0', '-mmt=off',
                    *options, str(destination), '.'], cwd=source, check=True)


for name, options in [
        ('lzma', ['-m0=LZMA', '-ms=off']),
        ('lzma2', ['-m0=LZMA2', '-ms=off']),
        ('solid', ['-m0=LZMA2', '-ms=on']),
        ('stored', ['-m0=Copy']),
        ('ppmd', ['-m0=PPMd:mem=16m', '-ms=on']),
        ('encrypted-headers', ['-pfixture-password', '-mhe=on']),
        ('encrypted-data', ['-pfixture-password', '-mhe=off']),
        ('unsupported-bzip2', ['-m0=BZip2']),
        ('unsupported-deflate', ['-m0=Deflate']),
        ('large-dictionary', ['-m0=LZMA2', '-mhc=off'])]:
    archive(name, root.parent / 'rar/input', *options)

for name, filename, data in [
        ('oversize-patch', 'large.sy1', b'x' * (4 * 1024 * 1024 + 1)),
        ('oversize-skipped', 'large.md', b'x' * (64 * 1024 * 1024 + 1)),
        ('empty-patch', 'empty.sy1', b''),
        ('no-patches', 'readme.md', b'Self-authored non-patch file.\n')]:
    with tempfile.TemporaryDirectory() as directory:
        (Path(directory) / filename).write_bytes(data)
        archive(name, directory, '-m0=LZMA2', '-md=1m')

# Advertise a 4-GiB LZMA2 dictionary with valid header CRCs. The reader must
# reject this before allocating decoder memory. Payload compression is unchanged.
path = root / 'large-dictionary.7z'
data = bytearray(path.read_bytes())
offset, size = struct.unpack_from('<QQ', data, 12)
start = 32 + offset
position = data.index(b'\x21\x21\x01', start, start + size) + 3
data[position] = 40
struct.pack_into('<I', data, 28, zlib.crc32(data[start:start + size]))
struct.pack_into('<I', data, 8, zlib.crc32(data[12:32]))
path.write_bytes(data)

# The uncompressed next header declares 65537 entries, exceeding the product's
# header-count limit. No large corpus or allocation is needed to test rejection.
header = b'\x01\x05\xc1\x01\x00\x00\x00'
start_header = struct.pack('<QQI', 0, len(header), zlib.crc32(header))
(root / 'too-many-entries.7z').write_bytes(
    b'7z\xbc\xaf\x27\x1c\x00\x04' +
    struct.pack('<I', zlib.crc32(start_header)) + start_header + header)
