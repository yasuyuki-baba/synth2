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
See [render state](render-state-separation.md), [voice orchestration](voice-orchestration.md)
and the [audio contract](contracts/audio-rendering.md). Validate dependency changes
with `tools/audit_dsp_headers.py`, and behavior with the loading/component suites.

Prepared `LfoBlock` storage is separate from the `Lfo` generator; read-only adapters
include only the data header. `Oscillator` supplies scalar phase/table processing;
`OscillatorSection` owns oscillator/stack/Sub composition and FM/Sync state. Keep
these declarations in separate headers without changing their processing order.
The arpeggiator consumes its narrow `ArpeggiatorSettings` value type rather than
RenderControls. The header audit checks independent compilation and forbidden
transitive dependencies for these boundaries as well as the DSP interfaces.
