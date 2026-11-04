# Patch loading contract

## Required behavior

Save Synth2 patches as version 200. Accept explicit legacy source versions 1–113
and supported named unversioned files, in addition to version 200.
This includes 99 and 101–103 through the legacy conversion path. The independently
observed native conversion paths cover 100 and 104–113, including 112; that
evidence range is not an input whitelist. Acceptance of other versions does not
establish original-instrument equivalence. Reject versions outside 1–113 and 200 and
malformed sources before replacing the current patch. Preserve original source integers and field presence;
an omitted field and an explicitly supplied zero are different inputs.

Version 200 is a complete snapshot of resolved host dials, retaining fractional
float values. It bypasses legacy file-version conversion. `ProcessorBridge::savePatch`
exports this format; `loadPatch` and library selection accept it. The explicit
Synth1 semantic export API continues to write integer version 113 files.
See [the version 200 format](../patch-format.md) for syntax and parameter coverage.
The patch format number is independent of processor state version 3 and of the
legacy p126 host parameter, whose ID and range remain unchanged.

Resolve source-version conversion at load time, then pass resolved controls to the
shared `Synth1PatchMapper`. Source 100/unversioned conversion uses the pre-105 and
105 strategies; legacy dependent conversion reads the original source context.
Source 105–108 discards legacy MIDI assignment words. Versions retain their own
extended-value rules, including p21=128 and 16-bit MIDI destination words.
Keep serialized source limits separate from host automation ranges and DSP limits.

For a sparse version 113 source, omitted controls use the native file-loading
source defaults, including filter decay/release 64, filter envelope depth 128,
amp attack/decay/release 64, amp sustain/gain 107, and velocity sensitivity 64.
Direct semantic parameter application is a different loading path and can retain
different values for omitted fields. Explicit source entries override defaults.

Direct normalized `.sy1` source buffers retain integer source-version conversion
by default. The processor explicitly selects continuous mapping for version 3
host controls; legacy version 1/2 states retain integer mapping. A host-mode choice
must not silently change the default interpretation of a source-buffer importer.

Keep source text/version, normalized host presentation, resolved controls and the
runtime-edit mask distinct. Explicit edits operate on runtime dials and must not
re-convert unrelated imported fields. Equal-value setters can still change loading
history and must not be discarded merely because the visible value is unchanged.
Select render policy from control meanings and observed history, never patch names,
bank ordinals, filenames or source hashes.

Bank selection is transactional. Invalid .sy1/ZIP input must preserve the previous
library, selected source and processor state. State recall must not require the
original external bank file. File and ZIP access occurs outside audio callbacks.
The processor starts with Init independently of the library. The library starts
empty, contains only explicitly imported sounds and has no privileged bank type.

## Verification

[LoadingBehaviorTest.cpp](../../tests/LoadingBehaviorTest.cpp) checks all accepted
versions and synthetic input rejection using generated inputs.

[PatchFormatTest.cpp](../../tests/PatchFormatTest.cpp) checks exact fractional
serialization, resolved legacy import/export, raw stereo replay, state recall and
transactional rejection for version 200. Library tests exercise the same files.


[PatchLibraryTest.cpp](../../tests/PatchLibraryTest.cpp) checks transactional bank
selection. [PatchImportWorkspaceTest.cpp](../../tests/PatchImportWorkspaceTest.cpp)
checks consistent ordinary/workspace conversion across accepted versions. See
[source conversion](../source-conversion.md) for strategy boundaries.

## Evidence and limits

Acceptance and exercised conversion cases do not establish correctness for every
historical bank. Agreement with a preceding implementation is a separate regression
criterion and cannot independently establish original-instrument correctness.

The [public test input description](validation.md) and linked product tests contain
the inputs/expectations needed for local verification without external assets.
