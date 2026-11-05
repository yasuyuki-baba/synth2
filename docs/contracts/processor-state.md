# Processor and state contract

## Required behavior

`ProcessorBridge` is the SDK-independent host boundary. Parameter IDs, ordering,
ranges and default values are stable contracts. Continuous GUI controls retain
fractional host values; selectors and switches use discrete source values.
Version 3 processor states render continuous controls without the Synth1 integer
patch conversion. Versions 1 and 2 retain the legacy integer mapping. Validate saved values
by stable parameter ID rather than XML child order or numeric text spelling.
States for Synth2 version 200 patches retain `patchFormat="200"` and use the
direct fractional mapping mode. Patch format 200 and processor state version 3
are separate schemas; the legacy p126 host range is unchanged.

Host/UI editing uses continuous parameters for continuous dials and integer steps
for selectors, switches, quantized tuning, voice counts, MIDI source/destination
words and serialized metadata. Fine tuning is continuous outside its neutral band;
route sensitivity p50/p51 is continuous, while source/destination p86–p89 is discrete.
Preserve source ranges: sparse choices such as
chorus 1/2/4 must not be replaced by menu ordinals or narrowed enum ranges.
Known choices have names; historical noncanonical values remain representable.
SDK editing steps do not quantize the loading adapter or saved bridge state;
opening the editor or synchronizing SDK values must not send parameter setters
back into the processor. Fractional historical selector values remain in bridge
state until explicitly edited, while SDK controls present integer resolution.

GUI/DAW parameter resolution is independent of MIDI message resolution. Ordinary
CC data remains 7-bit, pitch bend remains 14-bit and Synth1-compatible controller
routes retain their existing integer-dial conversion. Do not infer fractional
MIDI modulation or smoothing from continuous host parameter metadata.

Read the retained legacy binary XML state format. Preserve imported source text,
loading metadata, edits and program ordinal through save/restore; recall must work
without an external bank. Preserve the selected patch display name independently
of the library; legacy states without a name recover it from embedded source
text where available. Rejected patch loads and malformed state restores must
leave the current valid state intact. A state file is not a snapshot of running
oscillators, effect delay buffers, clocks or RNG execution history.

`ProcessorState` and its codec own the binary/XML document, validation and source
context reconstruction. `ProcessorBridge` snapshots controls for saving and
applies only a fully validated decoded state. The codec owns no engine, atomics,
audio buffers or receiver clocks. Parameter metadata is a separate value type.
Patch and state I/O share a private C-locale numeric conversion without changing
the host's global locale. State documents retain legacy finite-number spellings;
version 200 patch tokens retain their stricter decimal-only syntax and range checks.

Serialize host calls. `prepare` establishes sample rate and buffer capacity before
rendering. Valid process calls produce stereo output within prepared capacity.
Reject and silence oversized callbacks without reallocating or splitting DSP to
hide the error. Prepared zero-frame callbacks accept null output pointers and
retain queued MIDI. Preserve event offsets and equal-timestamp order.
Double output is an exact widening of the float synthesis path. MIDI publication
is bounded; rejected events and overflow must remain observable. When an overflowed
batch is consumed, replace it with channel-stop messages so a dropped Note Off
cannot leave its note held indefinitely. Do not silently keep a partial batch. The 60-second host tail declaration is approximate
metadata and must not truncate the rendered tail.

## Host lifecycle

Repeated program changes, state recall, note release and subsequent playback after
reprepare must remain valid within the documented lifecycle rules. Sample-rate
changes must not manufacture a note-on or promise continuation of every physical
history. Prepared capacity, zero-frame MIDI and oversized-callback rejection
follow the process contract above; conditional voice/output retention is defined
by the [output pipeline](../architecture/output-pipeline.md).

## Verification

The public `ParameterContract` in
[LoadingBehaviorTest.cpp](../../tests/LoadingBehaviorTest.cpp) independently pins
all host IDs, ordering, ranges and declared defaults.


Public ProcessorBridge/ProcessorState/ProcessorContracts tests use self-authored
sources, explicit public parameter identities and generated save/restore data.
They verify API rejection, queues, submitted-state equality and interface behavior.
The public tests require no previous-implementation state archive.

[StateRepresentationTest.cpp](../../tests/StateRepresentationTest.cpp) verifies
stable-ID binding with reversed XML children and equivalent fixed/scientific
numeric spellings for native-context and legacy states. Unknown and duplicate IDs
remain atomic rejections after reordering.

[HostLifecycleTest.cpp](../../tests/HostLifecycleTest.cpp) covers variable block
sizes/rates, generated-patch recall, dense voices, stop/retrigger and finite
output. These generated histories do not establish arbitrary host behavior.

## Limits

Readable processor state chunks do not by themselves establish DAW plugin-identity
compatibility. Actual plugin/DAW interoperability and concurrent SDK host behavior
require platform checks.

Legacy loading resolves compatible integer parameters, including defaults and
source-version conversion, before exact promotion to floating-point dials.
Subsequent fractional edits must not undergo legacy conversion again.

The native wrapper's v2 envelope adds instance browser state and a load/save edit
baseline around the existing processor state. It accepts wrapper v1. Both payloads
are validated before committing a recall; library paths are never needed to restore
the sounding processor. See [library/session behavior](../gui/patch-library.md).
