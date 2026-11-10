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
Preserve upstream LFO key/pedal tracking. Same-timestamp legato Off/On replacements retain
the sounding core through assignment, including partial/full ARP gates, bypass
binding changes and intervening same-time ordinary controllers. An unmatched
Note Off is flushed before audio advances; panic messages still take effect.
Deterministic LFO waveforms do not by themselves invalidate the running ARP clock.

Polyphonic admission collects the keyboard state for one timestamp before
starting voices. Admit new keys most-recent first, up to the effective physical
voice count; older pending keys must not consume cores or phase draws. A repeated
pending key retains its latest velocity/order, and a key cancelled before
admission must not start a voice. Preserve a pending key released under sustain
until pedal-up. Input traces and upstream keyboard order remain unchanged.

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

ARP absolute sample positions use integer frame counters, independently of the
Float32 sample-rate and tempo inputs. Very slow tempos saturate period/gate frame
counts before conversion, and long-running clocks must retain sub-block offsets.

Tempo and beat edits rescale the elapsed integer cycle counter using Float32
multiply/divide followed by integer truncation at each setter. Recompute gate
boundaries from that counter and the current period/gate, including live gate
edits that cross a previously queried boundary. Keep absolute clocks integer.
Do not substitute fractional pending deadlines or a Double scheduling path.

Ordered ARP selection retains the last keyboard key, octave and direction across
held-key membership edits. All generated notes retain the first-key trigger
velocity until the keyboard is reset. Bypass restores held keys in physical
press order; enabling releases bypassed keys through ordinary note events.

MIDI timestamps define input regions before the prepared/256-frame workspace
limit is applied. Restart that workspace budget at each input region while
keeping equal-timestamp event groups intact. Bend advances once per root span
of at most 32 frames; a large callback must not add an extra step by carrying
a workspace boundary across an intervening MIDI event.
