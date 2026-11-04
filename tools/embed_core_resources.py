#!/usr/bin/env python3
"""Embed source resources without any plugin/framework dependency."""
import argparse
import json
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
header='#pragma once\n#include <cstddef>\nnamespace synth2::resources {\n'
source='#include "CoreResources.h"\nnamespace synth2::resources {\n'
for symbol,name in [('initPatch','init.sy1')]:
 data=(a.root/'resources'/name).read_bytes()
 header+=f'extern const unsigned char {symbol}[];\nextern const std::size_t {symbol}Size;\n'
 source+=f'const unsigned char {symbol}[] = {{'+','.join(str(n) for n in data)+f'}};\nconst std::size_t {symbol}Size=sizeof({symbol});\n'
names=json.loads((a.root/'resources/parameter_names.json').read_text())
if not isinstance(names,list) or len(names)!=128 or not all(isinstance(name,str) and name for name in names):
 raise ValueError('parameter_names.json must contain 128 nonempty labels in host parameter order')
header+='extern const char* const parameterNames[128];\n}\n'
source+='const char* const parameterNames[128] = {'+','.join(json.dumps(name) for name in names)+'};\n}\n'
for name,content in [('CoreResources.h',header),('CoreResources.cpp',source)]:
 target=a.output/name
 if not target.exists() or target.read_text()!=content:target.write_text(content)
