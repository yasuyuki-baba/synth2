#!/usr/bin/env python3
"""Check portable C++ file/type spelling and setting-type suffixes without an SDK.

Scope is projects/Synth2/src, including the maintained patch core. This is a lexical
check, not a C++ parser: responsibility, identifier meaning and variable/function
spelling remain review concerns. SDK-required wrapper/resource names, tools,
third-party code and generated files are outside this tree.
"""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CPP_EXTENSIONS = ('.h', '.hpp', '.hh', '.hxx', '.cpp', '.cc', '.cxx')
PASCAL_CASE = re.compile(r'[A-Z][A-Za-z0-9]*\Z')
RETIRED_SUFFIX = re.compile(r'(?:Params|Parameters|Config)\Z')
# Consume literals/comments before identifiers, including arbitrary raw delimiters.
TOKEN = re.compile(
    r'(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)")'
    r'|(?P<comment>//(?:\\\r?\n|[^\n])*|/\*[\s\S]*?\*/)'
    r'|(?P<literal>(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*"|(?:u8|u|U|L)?\'(?:\\[\s\S]|[^\'\\])*\')'
    r"|(?P<number>[0-9](?:[A-Za-z_0-9.]|'(?=[A-Za-z_0-9]))*)"
    r'|(?P<identifier>[A-Za-z_][A-Za-z_0-9]*)|(?P<punctuation>\S)')


def declared_types(source):
    tokens = [(m.group(), m.start()) for m in TOKEN.finditer(source)
              if m.lastgroup in ('identifier', 'punctuation')]
    seen = set()
    for i, (word, _) in enumerate(tokens):
        if word not in ('class', 'struct', 'enum'):
            continue
        j = i + 1
        if word == 'enum' and j < len(tokens) and tokens[j][0] in ('class', 'struct'):
            j += 1
        # Attributes/alignment may appear between a class-key and its name.
        while j < len(tokens):
            if [t[0] for t in tokens[j:j+2]] == ['[', '[']:
                depth = 2
                j += 2
                while j < len(tokens) and depth:
                    if tokens[j][0] == '[':
                        depth += 1
                    elif tokens[j][0] == ']':
                        depth -= 1
                    j += 1
            elif tokens[j][0] == 'alignas' and j + 1 < len(tokens) and tokens[j+1][0] == '(':
                depth = 1
                j += 2
                while j < len(tokens) and depth:
                    if tokens[j][0] == '(':
                        depth += 1
                    elif tokens[j][0] == ')':
                        depth -= 1
                    j += 1
            else:
                break
        if j < len(tokens):
            name, offset = tokens[j]
            if re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', name) and offset not in seen:
                seen.add(offset)
                yield name, source.count('\n', 0, offset) + 1


def audit(source_root):
    files = sorted(p for p in source_root.rglob('*') if p.suffix in CPP_EXTENSIONS)
    if not files:
        raise ValueError(f'No portable C++ sources found under {source_root}')
    errors = []
    portable_paths = {}
    for path in files:
        relative = path.relative_to(source_root).as_posix()
        if not PASCAL_CASE.fullmatch(path.stem):
            errors.append(f'{relative}: file stem must use PascalCase')
        key = str(relative).casefold()
        if key in portable_paths:
            errors.append(f'{relative}: case-insensitive path collision with {portable_paths[key]}')
        portable_paths[key] = relative
        for name, line in declared_types(path.read_text(encoding='utf-8')):
            if not PASCAL_CASE.fullmatch(name):
                errors.append(f'{relative}:{line}: type {name!r} must use PascalCase')
            if RETIRED_SUFFIX.search(name):
                errors.append(f'{relative}:{line}: type {name!r}: use Settings for processing settings, '
                              'Controls for render controls, or Input/State/View for those roles')
    return files, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT, help='Product repository root')
    args = parser.parse_args()
    try:
        files, errors = audit(args.root / 'projects/Synth2/src')
    except (ValueError, OSError) as error:
        parser.exit(2, str(error) + '\n')
    if errors:
        parser.exit(1, '\n'.join(errors) + '\n')
    print(f'Naming lint passed: {len(files)} portable C++ files (including patch/core)')


if __name__ == '__main__':
    main()
