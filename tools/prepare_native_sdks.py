#!/usr/bin/env python3
"""Install the exact public native SDK revisions from dependencies.lock.json."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(directory, *arguments):
    return subprocess.check_output(
        ['git', '-C', str(directory), *arguments], text=True).rstrip('\n')


def prepare(root, sdk, output):
    lock = json.loads((root / 'dependencies.lock.json').read_text())
    revision = git(sdk, 'rev-parse', 'HEAD')
    if revision != lock['iplug2']['revision']:
        raise ValueError('iPlug2 checkout does not match dependencies.lock.json')
    resolved = {'product_commit': git(root, 'rev-parse', 'HEAD'), 'iplug2': revision,
                'product_dirty': bool(git(root, 'diff', '--name-only', 'HEAD',
                                          '--', '.', ':(exclude)iPlug2'))}
    for name, entry in lock['native_sdks'].items():
        if not re.fullmatch(r'[0-9a-f]{40}', entry['revision']):
            raise ValueError(f'{name}: expected a complete pinned commit')
        destination = (sdk / entry['path']).resolve()
        if not destination.is_relative_to(sdk):
            raise ValueError(f'{name}: SDK path escapes iPlug2')
        placeholders = {}
        for relative in git(sdk, 'ls-files', '--', entry['path']).splitlines():
            path = Path(relative).relative_to(entry['path'])
            if path.name.lower() in {'readme.txt', 'readme.md'}:
                placeholders[path.as_posix()] = subprocess.check_output(
                    ['git', '-C', str(sdk), 'show', 'HEAD:' + relative])
        if not (destination / '.git').exists():
            # The upstream checkout may contain a tracked readme placeholder.
            # Refuse to replace any locally installed SDK without Git metadata.
            if destination.exists() and any(p.name.lower() not in {'readme.txt', 'readme.md'}
                                            for p in destination.iterdir()):
                raise ValueError(f'{name}: SDK directory is already populated')
            destination.mkdir(parents=True, exist_ok=True)
            # Replace only verified upstream placeholder readmes with the SDK's
            # own files. Never discard a user's local SDK or edited placeholder.
            for placeholder in list(destination.iterdir()):
                relative = placeholder.relative_to(sdk).as_posix()
                # Compare Git-clean content, allowing a Windows checkout's
                # CRLF conversion while still rejecting edited placeholders.
                actual = git(sdk, 'hash-object', '--path=' + relative, str(placeholder))
                expected = git(sdk, 'rev-parse', 'HEAD:' + relative)
                if actual != expected:
                    raise ValueError(f'{name}: modified SDK placeholder')
                placeholder.unlink()
            git(destination, 'init', '--quiet')
            git(destination, 'remote', 'add', 'origin', entry['repository'])
            git(destination, 'fetch', '--depth', '1', 'origin', entry['revision'])
            git(destination, 'checkout', '--detach', 'FETCH_HEAD')
        if git(destination, 'rev-parse', 'HEAD') != entry['revision']:
            raise ValueError(f'{name}: installed SDK is at a different revision')
        for changed in git(destination, 'diff', '--name-only', 'HEAD').splitlines():
            if (changed not in placeholders or
                    (destination / changed).read_bytes() != placeholders[changed]):
                raise ValueError(f'{name}: SDK has tracked local changes')
        if entry.get('submodules'):
            git(destination, 'submodule', 'update', '--init', '--depth', '1',
                '--', *entry['submodules'])
            status = git(destination, 'submodule', 'status', '--', *entry['submodules'])
            if any(line[:1] != ' ' for line in status.splitlines()):
                raise ValueError(f'{name}: SDK submodule revision mismatch')
        # Keep the iPlug2 checkout's tracked scaffold readmes unchanged. They
        # are not SDK code; every other tracked SDK file must match its commit.
        for relative, content in placeholders.items():
            (destination / relative).write_bytes(content)
        resolved[name] = {'repository': entry['repository'],
                          'revision': entry['revision'],
                          'preserved_placeholder_readmes': sorted(placeholders)}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(resolved, indent=2) + '\n')
    return resolved


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iplug2', type=Path, default=ROOT / 'iPlug2')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(ROOT, args.iplug2.resolve(), args.output.resolve())


if __name__ == '__main__':
    main()
