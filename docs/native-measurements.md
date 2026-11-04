# Reference comparison methodology

Product requirements and reproducible public tests are defined in the
[contracts](README.md), [test guide](../tests/README.md) and
[test input description](contracts/validation.md). No original instrument is needed to build
or run the portable suite.

## Before/after regression procedure

Run these examples from the Synth2 root in a shell with Python 3 available.
The portable tools work on macOS, Windows and Linux; the Python helpers locate
both ordinary and `.exe` binaries. No original instrument or external patch bank
is involved.

```sh
cmake --preset core
cmake --build build-oos-core --target Synth2Probe Synth2LoadingProbe CompareWaves
```

### Prepare immutable inputs

Create a fresh example directory. This step refuses to replace existing inputs;
run it once, before changing the implementation:

```sh
python3 - <<'PY'
from pathlib import Path

inputs = Path("build-oos-core/comparison-example/inputs")
inputs.mkdir(parents=True, exist_ok=False)
case = inputs / "basic"
case.mkdir()
common = "\n57,0\n58,0\n59,0\n65,0\n66,0\n77,0\n"
(case / "001.sy1").write_text(
    "Owned donor\nver=105\n19,73\n29,80" + common, encoding="ascii")
(case / "002.sy1").write_text(
    "Owned target\nver=113\n0,1\n1,1\n5,32\n19,91\n"
    "25,0\n26,0\n27,127\n28,0\n29,80" + common, encoding="ascii")
(inputs / "cases.txt").write_text("basic\n", encoding="ascii")
(inputs / "midi.txt").write_text(
    "0 144 60 100\n66150 128 60 0\n", encoding="ascii")
PY
```

The MIDI file has four decimal integers per row: absolute sample offset, status
byte (including channel), data byte 1 and data byte 2. Keep rows in timestamp
order; equal-time rows retain their file order. `--midi-timeline` replaces the
probe's generated note events. It accepts Note On/Off, pitch bend and CC
1/64/120/121/123, not arbitrary MIDI messages.

### Capture each implementation

Run the following block with `before` while the preceding implementation is
built. Then make the implementation change, rebuild the same targets, and run
the block again with `after` in place of `before`. Do not regenerate the inputs
or reuse an output directory. For a documentation-only check, two captures of
the same build check repeatability, not regression preservation.

```sh
python3 - before <<'PY'
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path("tools").resolve()))
from measurement_common import built_executable, clean_environment, digest, run_logged

stage = sys.argv[1]
if stage not in ("before", "after"):
    raise SystemExit("Expected before or after")
work = Path("build-oos-core/comparison-example")
inputs = work / "inputs"
output = work / stage
output.mkdir()
probe = built_executable("build-oos-core", "Synth2Probe")
loading = built_executable("build-oos-core", "Synth2LoadingProbe")
env = clean_environment()
env.update(SYNTH2_PROBE_SAMPLE_RATE="44100", SYNTH2_PROBE_BLOCK_SIZE="64",
           SYNTH2_PROBE_VELOCITY="100")
run_logged(
    [loading, inputs, inputs / "cases.txt", output / "controls.tsv", "--dsp-routes"],
    output / "loading.log", env=env)
run_logged(
    [probe, "-p", inputs / "basic/002.sy1",
     "--initial-preset", inputs / "basic/001.sy1", "--preset-warmup-blocks", "1",
     "--transactional-preset-load", "--midi-timeline", inputs / "midi.txt",
     "--float-wav", "-d", "3.5", "--tempo", "120", "-o", output / "audio.wav"],
    output / "render.log", env=env)
record = {
    "source_commit": subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True).strip(),
    "worktree": subprocess.check_output(
        ["git", "status", "--porcelain"], text=True),
    "binaries": {p.name: digest(p) for p in (probe, loading)},
    "inputs": {str(p.relative_to(inputs)): digest(p)
               for p in sorted(inputs.rglob("*")) if p.is_file()},
    "sample_rate": 44100, "block_size": 64, "tempo": 120,
}
(output / "context.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
print(output)
PY
```

Retain the compiler/version, architecture, build flags and build logs alongside
these outputs. A commit ID alone does not identify a dirty source tree; preserve
the source diff or an exact source snapshot for each capture.

`Synth2LoadingProbe CASE_ROOT CASE_LIST OUTPUT_TSV --dsp-routes` reads one case
directory name per whitespace-separated entry in the case list. Each directory
must contain `001.sy1` (donor) and `002.sy1` (target). This mode loads both with a
64-frame silent callback after each, then renders at 44,100 Hz and 120 BPM. It
starts note 60 at velocity 100 at sample 0, releases it at 66,150 and ends at
154,350. Its note timeline matches the example MIDI file, but the tools have
different initialization ordering: the loading probe prepares before loading the
donor, while the audio probe loads the donor before preparing. Compare each tool
against its own preceding result; these are not paired observers of one renderer.
The loading probe does not accept an arbitrary MIDI timeline.

The TSV has four columns: case, elapsed sample offset, field name and value.
It emits all 99 resolved source controls (`control_0` through `control_98`) and
selected DSP route/settings fields at the initial, changed and designated
boundary snapshots. These are not every member of `RenderControls` or a complete
receiver-state trace. This mode requires sources supported by the native resolver.
The probe's default mode instead requires `001.sy1`, `002.sy1` and `003.sy1` and
reports source values and typed-import adaptation; do not treat that output as
the resolved native control set.

### Compare without transformations

```sh
python3 - <<'PY'
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path("tools").resolve()))
from measurement_common import digest, raw_stereo_nrmse, read_wav

work = Path("build-oos-core/comparison-example")
before, after = work / "before", work / "after"
contexts = [json.loads((p / "context.json").read_text()) for p in (before, after)]
if contexts[0]["inputs"] != contexts[1]["inputs"]:
    raise SystemExit("Input bytes changed")
for directory in (before, after):
    rate, channels, values, _ = read_wav(directory / "audio.wav")
    if (rate, channels, len(values)) != (44100, 2, 154350 * 2):
        raise SystemExit("Unexpected WAV dimensions")
    if not any(values):
        raise SystemExit("Example rendered only silence")
failed = []
for name in ("audio.wav", "controls.tsv"):
    same = (before / name).read_bytes() == (after / name).read_bytes()
    print(name, "IDENTICAL" if same else "DIFFERENT",
          digest(before / name), digest(after / name))
    if not same:
        failed.append(name)
print("Raw stereo NRMSE:",
      raw_stereo_nrmse(before / "audio.wav", after / "audio.wav"))
if failed:
    raise SystemExit("Regression differences: " + ", ".join(failed))
PY
```

Investigate a difference; do not regenerate expectations or align/scale the audio
to obtain a pass. `CompareWaves reference.wav candidate.wav --window 0` is an
optional diagnostic, not this acceptance check: it still downmixes for its
analysis, and its exit status is not a byte-identity assertion.

This example checks one sparse patch transition and one note. It is not a complete
loading/render-control oracle, repeated-note/controller matrix or replacement for
the applicable product suite. Add cases and exact timelines for the behavior
being changed, and use additional observations when the selected TSV fields do
not cover it.

## Independent original-instrument observations

When reporting an independent original-instrument observation, identify the exact
binary, sample rate, block configuration, patch/loading history and MIDI timeline.
Retain raw complete stereo and file hashes. Aligned, downmixed or gain-fitted
metrics are diagnostics and cannot replace raw comparison. An experiment supports
its sampled conditions; it does not automatically become a product requirement.

Use the [product tools](../tools/README.md) for candidate rendering and diagnostics.
Keep numerical operation order and state lifetime intact during refactors, and
compare candidate WAV bytes and loaded controls with the preceding implementation.
