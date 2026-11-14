"""Regenerate self-authored archive fixtures; tests need no external RAR tool.

Usage: python3 generate.py /path/to/rar-6.24
RAR 6.24 is used because it can write both RAR4 and RAR5 archives.
The encoder is a development tool only and is not redistributed with Synth2.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent
encoder = str(Path(sys.argv[1]).resolve())


def archive(name, source, *options):
    destination = root / (name + '.rar')
    destination.unlink(missing_ok=True)
    subprocess.run([encoder, 'a', '-idq', '-r', '-m5', '-md1m', *options,
                    str(destination), '.'], cwd=source, check=True)


for name, options in [
        ('compressed-rar4', ['-ma4', '-s-']),
        ('stored-rar4', ['-ma4', '-s-', '-m0']),
        ('compressed-rar5', ['-ma5', '-s-']),
        ('solid-rar5', ['-ma5', '-s']),
        ('solid-rar4', ['-ma4', '-s']),
        ('encrypted-rar5', ['-ma5', '-hpfixture-password'])]:
    archive(name, root / 'input', *options)

for name, filename, data in [
        ('oversize-rar5', 'large.sy1', b'x' * (4 * 1024 * 1024 + 1)),
        ('oversize-skipped', 'large.md', b'x' * (64 * 1024 * 1024 + 1)),
        ('empty-patch', 'empty.sy1', b''),
        ('no-patches', 'readme.md', b'Self-authored non-patch file.\n')]:
    with tempfile.TemporaryDirectory() as directory:
        (Path(directory) / filename).write_bytes(data)
        archive(name, directory, '-ma5')
