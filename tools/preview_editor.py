#!/usr/bin/env python3
"""Generate a self-contained, interactive GUI study from the native layout."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def patch(text):
    values = {}
    title = ""
    for line in text.splitlines():
        line = line.split("#", 1)[0].split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("ver="):
            values[126] = int(line[4:])
        elif "," in line:
            key, value = line.split(",", 1)
            try:
                key = int(key)
                value = float(value)
            except ValueError:
                continue
            if 0 <= key < 128:
                values[key] = value
        elif not title:
            title = line
    return {"name": title, "values": values}


def load_model(exporter):
    model = json.loads(subprocess.check_output([str(exporter.resolve())], text=True))
    model["names"] = json.loads((ROOT / "resources/parameter_names.json").read_text())
    model["init"] = patch((ROOT / "resources/init.sy1").read_text())
    model["init"]["values"] = {index:parameter["initial"] for index,parameter in enumerate(model["parameters"])}
    return model


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", type=Path, default=ROOT / "build-oos-core/ExportEditorLayout")
    parser.add_argument("--output", type=Path, default=ROOT / "build/editor/synth2-preview.html")
    args = parser.parse_args()
    model = load_model(args.exporter)
    template = (ROOT / "tools/editor-preview.html").read_text()
    encoded = json.dumps(model, ensure_ascii=False).replace("<", "\\u003c")
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(template.replace("/*EDITOR_MODEL*/null", encoded))
    print(output)


if __name__ == "__main__":
    main()
