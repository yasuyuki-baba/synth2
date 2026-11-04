# Source-version conversion architecture

Patch parsing, source-version conversion, value mapping, render-policy selection
and DSP processing have separate responsibilities. Select an immutable conversion
strategy once per load, then use the shared `Synth1PatchMapper` across versions.
Do not spread source-version checks into DSP kernels or create version-named
mapper subclasses to encode loading rules.

| Source | Conversion contract |
| --- | --- |
| 100 / supported named unversioned | Pre-105 curves followed by 105 conversion; dependent fields read original source |
| 104 | Its selected conversion table; MIDI assignment words retained |
| 105–106 | Versioned curves and original-source sensitivity conversion; legacy MIDI assignments discarded; p21=128 resolves to 127 |
| 107–108 | Legacy dependent conversion and discarded MIDI assignments; p21=128 retained |
| 109–113, including 112 | Selected version's table, extended MIDI words and p21=128 |

The loader accepts explicit versions 1–113. Versions outside the native conversion
table above, including 99 and 101–103, retain the legacy pre-105 conversion path.
Their acceptance does not imply independently verified original-engine behavior.

Source 105–108 resets phase dials p91/p92 to zero. FM p45 retains its source dial.
Source 105–106 resets Unison spread p84 to 64; source 104 and 107–113 preserve
explicit spread values. The omitted spread default is 64. These conversions do
not replace the original source metadata.

Keep source presence, resolved controls and render values separate. Reject versions
outside the accepted range and invalid fields before mutating processor state. Serialized source
limits, public host ranges and DSP limits are independent; preserving a source
integer does not require expanding host automation ranges.

The parser/model lives in `projects/Synth2/src/patch/core`. Conversion strategies
are standard-library code with no SDK or runtime voice/clock ownership.
See [patch loading](../contracts/patch-loading.md) for required behavior and tests.

## Patch-core API boundaries

- The parser preserves serialized entries and the optional source version.
- `resolveNativePatchControls` overlays source values onto native defaults and
  applies source-version conversion. It returns 99 effective controls separately
  from source version and explicit field presence, for source 100, 104–113 and
  named unversioned files.
- `importNativeSynth1Patch` resolves and decodes that complete context.
  `decodeNativePatchControls` decodes an already resolved context without repeating
  source-family conversion.
- `importSynth1Patch` applies family-specific remaps and implicit defaults before
  typed decoding. It also serves accepted versions outside the native resolver's
  range. Its optional-field policy differs from the complete native default
  overlay; the two import paths are not interchangeable.
- `Synth1Patch::source` retains original version and explicit parameter presence
  independently of resolved or implicit values. Programmatically constructed
  models have no source metadata.
- `exportSynth1Patch` writes canonical modern-format typed values, not a lossless
  reconstruction of the original file.

`Synth1PatchLegacyImport.cpp` owns the family-normalizing API and its workspace
overload. Historical value-combination overrides belong to that model conversion,
not the complete native resolver or resolved-control decoder. They are not a
general specification of DLL loading; retain their existing behavior until
independent conversion evidence supports replacing them. Ordinary native file
loads use the complete resolver; direct semantic import and legacy non-native
states retain their distinct optional-field contract.

The loading/host adapters own transactional acceptance, source text, runtime edits
and saved-state metadata. The patch core does not own voices, clocks, random
streams or host automation ranges. See [processor/state](../contracts/processor-state.md)
for saved-state ownership.

## Runtime workspace

ProcessorBridge owns the prepared mapper. Parsing, file access and state
serialization stay outside audio callbacks. Runtime dial mapping reuses a
mapper-owned source/import workspace. Initialize
legacy immutable import maps before audio processing; resetting a source-version
strategy must invalidate its cache/controller context without allocating a new
mapper in the callback. See the [verification map](../validation/coverage.md).

Ordinary and workspace import/decode overloads share conversion semantics.
Prepare `Synth1PatchImportWorkspace` outside audio callbacks so repeated
runtime-dial adaptation can reuse storage.
