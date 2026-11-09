# Product development tools

These tools use the portable core and do not require an original Synth1 DLL or
compatibility checkout. Native APP capture is an optional Wine GUI workflow.

| Task | Entry point |
| --- | --- |
| Build and run product contracts | [tests/README.md](../tests/README.md) |
| Render patches and exact MIDI histories | [Synth2Probe](synth2_probe.cpp) |
| Inspect resolved controls and selected DSP routes | [Synth2LoadingProbe](synth2_loading_probe.cpp) |
| Compare WAVs for diagnostics | [CompareWaves](compare_waves.cpp) |
| Audit DSP, voice, editor and state header compilation/dependency boundaries | audit_dsp_headers.py; GNU-style GNU/Clang Ninja/Makefile builds also provide CTest `HeaderBoundaryAudit` and target `Synth2HeaderAudit` |
| Check portable C++ file/type names and setting suffixes | `python3 tools/lint_naming.py` or CMake target `Synth2NamingLint` |
| Preview/render the editor study | preview_editor.py, render_editor_study.py |
| Capture the Windows APP editor | capture_iplug_wine.py |

Use the [reference comparison procedure](../docs/validation/reference-comparisons.md) for a
self-authored input example, exact MIDI timeline format, loading-probe file layout
and byte-for-byte before/after checks. The loading probe is a case-based tool, not
a single-patch command; its mode determines which values it reports.

Editor generation is described in the [GUI guide](../docs/gui/README.md); native
APP capture is covered by the [platform guide](../docs/validation/manual-platform.md#native-windows-app-capture).

`measurement_common.py` supplies standard-library WAV decoding, hashes, raw stereo
errors and logged subprocess I/O. Its tests are part of the portable suite.
Aligned/downmixed/gain-fitted diagnostics from CompareWaves cannot replace raw
stereo compatibility evidence.

Configure with `SYNTH2_BUILD_TOOLS=ON` to build the offline tool targets; the
portable `core` preset enables them. Native presets disable tools. Tests enabled
with `SYNTH2_BUILD_TOOLS=OFF` still build the required Probe and WAV comparator,
but omit LoadingProbe, the editor exporter and the host benchmark.

Native CI helpers: `prepare_native_sdks.py` installs lock-file SDK revisions and
writes a build-identity manifest. `package_native.py` checks the expected native
products and creates a Windows ZIP or macOS tar.gz with metadata and notices.
See [CI](../docs/ci.md) for local commands and verification boundaries.

## Offline host benchmark

`Synth2HostBenchmark` is a non-gating offline diagnostic. It exercises explicitly
specified dry/wet synthetic profiles, checks core occupancy and audio, and reports
callback timing separately from setup and validation work. Use fresh CSV outputs,
record compiler/binary/CPU/run context and compare like-for-like conditions.

```sh
cmake --preset core
cmake --build build-oos-core --target Synth2HostBenchmark
./build-oos-core/Synth2HostBenchmark build-oos-core/host-benchmark.csv 128
```

On Windows, invoke `build-oos-core/Synth2HostBenchmark.exe` instead. The CSV path
must not exist; choose a fresh filename for each run.

Offline over-budget observations are not detected audio-device underruns. Neither
average nor sampled maximum callback time establishes a hard real-time deadline.
Actual DAW/device testing and concurrency contracts require separate validation.

`Synth2Probe --oversampling FACTOR` selects 1, 2, 4, 8 or 16 before preparation.
The default is 1. Higher-factor WAVs include processing latency; do not silently
align or gain-fit them when reporting raw audio comparisons.

## Isolated rendering diagnostics

Normal product targets and `Synth2Probe` do not read DSP bypass environment
variables. To isolate envelopes, filters or output effects, explicitly build
`cmake --build build-oos-core --target Synth2DiagnosticProbe`. This optional target
links a separate diagnostic core and is excluded from normal builds. Invoke it
with `--bypass-env`, `--bypass-filter` or `--bypass-fx`; the normal probe rejects
these switches. Bypass state belongs to the diagnostic rendering thread, is set
outside processing callbacks, and is neither a patch control nor saved processor state.
`--bypass-env` retains the probe's accompanying isolation dial overrides.

`Synth2RenderProfile` profiles generated bridge playback scenarios without a
pass/fail timing bound. See [host validation](../docs/validation/manual-platform.md) for the
command and interpretation of its per-case CSV and final-block hashes.
