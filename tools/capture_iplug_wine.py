#!/usr/bin/env python3
"""Capture the running iPlug2 Windows APP on an isolated Xvfb display.

Requires Wine, Xvfb, working local sockets and OpenGL. This captures the native
editor; it does not render or modify a design mockup. Audio/MIDI I/O is disabled.
"""
import argparse
import os
from pathlib import Path
import select
import re
import time
import shutil
import subprocess
import tempfile


def capture(args):
    executable = args.exe.resolve()
    output = args.output.resolve()
    if not executable.is_file() or executable.read_bytes()[:2] != b'MZ':
        raise RuntimeError('Expected a built Windows PE executable (--exe).')
    if args.capture_method == 'sdk' and ' ' in str(output):
        raise RuntimeError('The pinned SDK screenshot argument does not support spaces in the output path.')
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise RuntimeError('Output already exists; choose a new capture path.')
    env = os.environ.copy()
    env.update(WINEDEBUG='-all', WINEARCH='win64', LIBGL_ALWAYS_SOFTWARE='1',
               WINELOADER=str(Path(args.wine).resolve()))
    server = shutil.which('wineserver')
    if not server:
        sibling = Path(args.wine).resolve().parent/'wineserver64'
        if sibling.is_file():
            server = str(sibling)
    if server:
        env['WINESERVER'] = server
    elif not args.prefix:
        raise RuntimeError('Cannot locate wineserver for automatic cleanup; provide an explicit dedicated --prefix.')
    # Keep the capture server and prefix separate from other running sessions.
    with tempfile.TemporaryDirectory(prefix='synth2-wine-capture-') as temporary:
        env['WINEPREFIX'] = str(args.prefix.resolve()) if args.prefix else temporary+'/prefix'
        read_fd, write_fd = os.pipe()
        xserver = None
        try:
            with output.with_suffix('.xvfb.log').open('w') as log:
                xserver = subprocess.Popen(
                    [args.xvfb, '-displayfd', str(write_fd), '-screen', '0',
                     '1440x1080x24', '-nolisten', 'tcp'],
                    pass_fds=(write_fd,), stdout=log, stderr=subprocess.STDOUT)
                os.close(write_fd)
                write_fd = None
                if not select.select([read_fd], [], [], 10)[0]:
                    raise RuntimeError('Xvfb did not report a display within 10 seconds.')
                display = os.read(read_fd, 64).decode().strip()
                if not display.isdigit():
                    raise RuntimeError('Xvfb could not start. Check the .xvfb.log (local socket permission is required).')
                env['DISPLAY'] = ':'+display
                windows_path = 'Z:'+str(output).replace('/', '\\')
                with output.with_suffix('.wine.log').open('w') as wine_log:
                    if args.capture_method == 'sdk':
                        try:
                            result = subprocess.run(
                                [args.wine, str(executable), '--no-io', '--screenshot', windows_path],
                                env=env, cwd=executable.parent, stdout=wine_log,
                                stderr=subprocess.STDOUT, timeout=args.timeout)
                        except subprocess.TimeoutExpired as error:
                            raise RuntimeError('Wine screenshot timed out; check the .wine.log.') from error
                        if result.returncode or not output.is_file():
                            raise RuntimeError(f'Wine capture failed (exit {result.returncode}); check the .wine.log.')
                    else:
                        # Wine PrintWindow can omit the OpenGL child. Read the
                        # visible X11 pixels in the app window's screen region.
                        process = subprocess.Popen(
                            [args.wine, str(executable), '--no-io'], env=env,
                            cwd=executable.parent, stdout=wine_log, stderr=subprocess.STDOUT)
                        try:
                            deadline = time.monotonic()+args.timeout
                            geometry = None
                            while time.monotonic()<deadline and process.poll() is None:
                                tree = subprocess.run(['xwininfo','-root','-tree'],env=env,
                                                      capture_output=True,text=True,timeout=5).stdout
                                match = re.search(r'"Synth2"[^\n]*? (\d+x\d+[+-]\d+[+-]\d+)',tree)
                                if match:
                                    geometry = match.group(1)
                                    break
                                time.sleep(.25)
                            if not geometry:
                                raise RuntimeError('The native Synth2 window did not appear; check the .wine.log.')
                            time.sleep(3)
                            if process.poll() is not None:
                                raise RuntimeError('The application exited before capture; check the .wine.log.')
                            subprocess.run(['import','-window','root','-crop',geometry,str(output)],
                                           env=env,check=True,timeout=10)
                        finally:
                            if process.poll() is None:
                                process.terminate()
                                try:
                                    process.wait(timeout=5)
                                except subprocess.TimeoutExpired:
                                    process.kill()
                                    process.wait()
                with output.open('rb') as png:
                    if png.read(8) != b'\x89PNG\r\n\x1a\n':
                        raise RuntimeError('The capture is not a PNG file.')
                print(output)
        finally:
            os.close(read_fd)
            if write_fd is not None:
                os.close(write_fd)
            # A dedicated prefix belongs to this capture. Never stop an explicitly
            # supplied prefix: the caller may also be using it for other programs.
            if not args.prefix:
                if server:
                    subprocess.run([server, '-k'], env=env, timeout=10,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if xserver and xserver.poll() is None:
                xserver.terminate()
                try:
                    xserver.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    xserver.kill()
                    xserver.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--wine', default=shutil.which('wine64') or shutil.which('wine'))
    parser.add_argument('--xvfb', default=shutil.which('Xvfb'))
    parser.add_argument('--prefix', type=Path, help='Optional dedicated, pre-initialized Wine prefix')
    parser.add_argument('--timeout', type=int, default=90)
    parser.add_argument('--capture-method', choices=['desktop','sdk'], default='desktop',
                        help='Desktop pixels include the OpenGL child; SDK PrintWindow may omit it under Wine.')
    args = parser.parse_args()
    if not args.wine or not args.xvfb:
        parser.error('Wine and Xvfb must be installed or provided explicitly.')
    if args.capture_method == 'desktop' and (not shutil.which('import') or not shutil.which('xwininfo')):
        parser.error('Desktop capture requires ImageMagick import and xwininfo.')
    try:
        capture(args)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
