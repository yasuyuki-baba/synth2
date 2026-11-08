# MIDI, arpeggiator and voice contract

## Required behavior

Preserve the exact MIDI timeline, including sub-block offsets and equal-timestamp
ordering. Native keyboard/performance input uses one omni state. Keep sustained
upstream keys separate from generated ARP notes and Voice note flags. Routed
controller offsets belong to `ControllerRuntime` and callback adaptation; they
must not enter source getters, mapper caches or saved patch values. Restore the
mapped render controls at the callback boundary after applying performance offsets.

CC121 resets ordinary bend separately from routed pitch, wheel and pedal state.
Equal-value controller setters may still split rendering or restart receivers.
Keep per-note velocity gain and actual physical-core ownership through program
changes. Combine root region limits before advancing shared modulation state.

Native ARP stop clears held keys and pedal state, but restarts the empty clock.
Random order consumes its shared draw; ordered modes do not draw random values.
An absent-key Note Off can also restart that clock. Ordered and admitted random
ARP retain empty-keyboard render boundaries at partial and full gate. Full gate
has one boundary per period, including while existing voices are releasing.
Preserve upstream LFO key/pedal tracking. Connected full-gate assignments retain
the observed note-off/on ordering without releasing the core between assignments.
Deterministic LFO waveforms do not by themselves invalidate the running ARP clock.

Mono and poly modes share the 32-core physical pool. Unison consumes physical
cores from that budget; logical note counts do not create an independent pool. Mono admission pauses higher
core groups without destroying receiver history. A changed effective voice count
resets the pool allocation cursor; an equal-count load preserves it. Neither
operation implies erasing the retained physical state.

Logical note retirement is separate from VCA/filter/FM history. Reuse decisions
inspect actual physical VCA activity. Consume pending Retrigger/Legato/Retained-
Retrigger and glide transitions at their defined start/prepare boundaries;
retirement alone must not erase a pending transition or reset every receiver.

## Verification and limits

[PerformanceBehaviorTest.cpp](../../tests/PerformanceBehaviorTest.cpp) checks
sample-accurate dispatch, equal-time order, sustain, CC121 and source/state
isolation with generated MIDI. Dispatcher markers isolate event timing from
callback-sensitive DSP histories; they do not assert whole-engine waveform
identity across arbitrary callback partitions.


Single-note results do not establish repeated-note, arbitrary live-edit or all
controller behavior. See [voice lifecycle](../architecture/voice-lifecycle.md) for ownership.
