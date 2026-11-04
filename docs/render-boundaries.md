# Mapping, policy and component boundaries

The mapper translates source semantics to render values. Render policy selects
implementation and receiver composition from named controls and explicit history.
Per-component adapters provide narrow immutable settings and caller-owned state.
No decision may depend on patch title, bank identity, ordinal or source hash.

`RenderPolicyInputs` must not import a patch model, SDK parameter object, source
version or indexed source array. Only loading/host adapters decode serialized
control indices. Initialize mapped controls, then choose common receiver
composition or the ordered restrictions for unadmitted random histories.
Deterministic sources and mapped oscillator envelopes do not require source-format
admission; random sources require both source admission and valid running history.
Preserve stack/sub mapping and modulation-control preparation order.
Common composition selects receivers from destinations and valid history;
it need not execute fallback guards whose flags would be overwritten. Keep
resonant-noise reuse and idle filter-reset decisions explicit and shared by both
paths. Unknown random histories retain the ordered fallback rules.

`render/VoiceConfiguration.h` adapts the render aggregate to oscillator, filter
and modulation controls. Controllers borrow const LFO views and cannot advance
clocks or own shared randomness. Engine owns scheduling and shared state; Voice
owns physical slots and per-core execution. Component headers must compile
independently without Engine, Voice, patch or the full render aggregate.

Preserve all-core envelope/glide/FM/filter order and each receiver's boundaries.
See [voice orchestration](voice-orchestration.md)
and the [audio contract](contracts/audio-rendering.md). Validate dependency changes
with `tools/audit_dsp_headers.py`, and behavior with the loading/component suites.

Prepared `LfoBlock` storage is separate from the `Lfo` generator; read-only adapters
include only the data header. `Oscillator` supplies scalar phase/table processing;
`OscillatorSection` owns oscillator/stack/Sub composition and FM/Sync state. Keep
these declarations in separate headers without changing their processing order.
The arpeggiator consumes its narrow `ArpeggiatorSettings` value type rather than
RenderControls. The header audit checks independent compilation and forbidden
transitive dependencies for these boundaries as well as the DSP interfaces.

## Render controls and loading-state ownership

`RenderControls` contains render controls only. Source version, normalized source
values, omitted-field masks and mapper cache belong to loading/mapping adapters.
Load-history context is separate from both. Diagnostic `MappedSynthSnapshot`
objects must never become Engine or Voice parameters.

Mapper cache hits require matching destination, source values, presence, native
context and relevant history. A different destination forces remapping. Keep the
destination alive while using the cache; replacing an object in place requires
explicit remapping or replacing the mapper. Runtime performance offsets do not
belong in the cache or original source getters.

The bridge publishes/flushes load history before mapping and supplies its validity
explicitly to policy. Engine applies context at its clock/reload boundaries;
Voice does not receive a patch document or source version. State serialization
preserves loading/source metadata, not diagnostic snapshots or running DSP memory.
See [state](contracts/processor-state.md), [modulation](contracts/modulation.md)
and [loading](contracts/patch-loading.md) for behavior and regression entry points.
