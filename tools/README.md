# Product development tools

These tools use the portable core and do not require an original Synth1 DLL or
compatibility checkout. Native APP capture is an optional Wine GUI workflow.

| Task | Entry point |
| --- | --- |
| Build and run product contracts | [tests/README.md](../tests/README.md) |
| Render patches and exact MIDI histories | [Synth2Probe](synth2_probe.cpp) |
| Inspect resolved controls and selected DSP routes | [Synth2LoadingProbe](synth2_loading_probe.cpp) |
| Compare WAVs for diagnostics | [CompareWaves](compare_waves.cpp) |
| Export bank states, normalized host values and audio | [ExportBridgeBaseline](export_bridge_baseline.cpp) (fresh output only) |
| Audit DSP and voice header compilation/dependency boundaries | audit_dsp_headers.py; GNU-style GNU/Clang Ninja/Makefile builds also provide CTest `HeaderBoundaryAudit` and target `Synth2HeaderAudit` |
| Check portable C++ file/type names and setting suffixes | `python3 tools/lint_naming.py` or CMake target `Synth2NamingLint` |
| Preview/render the editor study | preview_editor.py, render_editor_study.py |
| Capture the Windows APP editor | capture_iplug_wine.py |

Use the [reference comparison procedure](../docs/native-measurements.md) for a
self-authored input example, exact MIDI timeline format, loading-probe file layout
and byte-for-byte before/after checks. The loading probe is a case-based tool, not
a single-patch command; its mode determines which values it reports.

`ExportBridgeBaseline OUTPUT_DIRECTORY BANK_FILE` exports an explicitly supplied .sy1/ZIP bank to a new
directory: parameter metadata, source patches, saved states, normalized host
values (`.values`) and interleaved stereo float audio (`.f32`). It uses a fixed
44,100 Hz / 64-frame rendering scenario. Normalized host values are not resolved
source controls or the complete render aggregate.

For offline timing, see the [host benchmark](../docs/host-validation.md). Editor
generation and native capture instructions are in the [GUI guide](../docs/gui/README.md).

`measurement_common.py` supplies standard-library WAV decoding, hashes, raw stereo
errors and logged subprocess I/O. Its tests are part of the portable suite.
Aligned/downmixed/gain-fitted diagnostics from CompareWaves cannot replace raw
stereo compatibility evidence.

Configure with `SYNTH2_BUILD_TOOLS=ON` to build the offline tool targets; the
portable `core` preset enables them. Native presets disable tools. Tests enabled
with `SYNTH2_BUILD_TOOLS=OFF` still build the required Probe and WAV comparator,
but omit LoadingProbe, editor/baseline exporters and the host benchmark.

Native CI helpers: `prepare_native_sdks.py` installs lock-file SDK revisions and
writes a build-identity manifest. `package_native.py` checks the expected native
products and creates a Windows ZIP or macOS tar.gz with metadata and notices.
See [CI](../docs/ci.md) for local commands and verification boundaries.
