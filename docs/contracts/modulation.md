# Modulation and execution-history contract

## Required behavior

Engine owns shared LFO sources, clocks, routing history and the CRT random stream.
ARP draws remain bound to that same stream and retain their position in the
scheduler. Phase/VCO randomness remains separate from the Engine-owned LFO pair. Do not fit seeds to patches or add draws to conceal a mismatch.

History setters, receiver queries and clock advances are distinct ordered actions.
Preserve callback/receiver spans, slot order, random advancement, fractional-rate
handling and state retention. Disabled shared sources may still advance their
native histories. A clock being eligible for a receiver and its running history
being valid are separate decisions.

Consumers use read-only clock/random/block views. They must not advance shared
clocks, reset them or draw additional random values. Key-synchronized sources are shared across all voices. The first held
upstream key resets phase; overlapping keys do not. Generate waveform signals
after MIDI dispatch, including events inside a host block. Retain fallback random
targets and its running stream across key resets without note-based reseeding. Reprepare does not unconditionally clear held keys or
running native history; apply the documented lifecycle and observed transition.

Source/loaded controls, explicit same-value edits and observed initialization
history remain distinct. Equal visible control values do not establish equal
clock state. Admission guards must depend on supported histories and control
meanings; unresolved histories must not silently be presented as native parity.

During a full load, MIDI routing re-dispatches the loaded rate using the retained
sync mode. The ordinary rate setter follows those re-dispatches, then the sync
setter applies the new mode and re-applies the rate. Preserve that order even when
the resulting visible rate or sync value is unchanged.

History publication uses bounded storage. A full queue rejects the new event,
counts the drop and retains already accepted events in FIFO order. Slots can be
reused after consumption. Once history is lost, subsequent rendering must not
claim the original random-history path is valid. Draining the queue permits new
notifications; it does not reconstruct discarded history or clear the cumulative
drop count.

Deterministic shared LFO depth retains fractional dials for sine, triangle, saw
and square receivers. Depth mapping retains the compatible whole-dial knots and
held spans. Fractional positions combine adjacent depth responses and use the earlier
held boundary, while borrowing the same clock and leaving random history intact.
Resolved deterministic LFOs and supported oscillator-envelope destinations use
common receiver composition for every source format. Source-version conversion
remains a loading concern; a format alone must not select a second modulation,
oscillator, tuning or filter algorithm after mapping. This includes disabled LFOs,
tempo sync and overlapping keys. Random receivers still require both admitted
source loading and valid running history. Unadmitted legacy random ARP retains
its existing composition restrictions even with deterministic or disabled LFOs.
Deterministic amplitude receivers use
the common control/VCA path even beside an unadmitted random source. This does not validate
unknown random histories. When shared and fallback amplitude slots coexist,
route each exactly once through its physical VCA input, preserving two independent
attenuation factors rather than a second sample-gain multiplication.
Random held and smoothed targets retain real current/previous levels. Each
nonzero effective range uses one shared CRT draw and projects that same word
through compatible endpoint knots; a zero range consumes no draw. Queries never
advance the stream. Fractional depth edits overlay the observed integer setter
history without redrawing held targets. Subsequent draws and key-sync resets use
the fractional range. Free-rate dials retain fractions through compatible Hz
knots, including both shared slots and sync-mode restoration. Tempo sync selects
a discrete musical division while retaining the free dial. Phase counters, sample
periods and the CRT integer recurrence remain separate boundaries. Compatible target projection and downstream receiver curves
can retain plateaus; storing a fractional edit does not guarantee a distinct
audio result for every pair of nearby settings.

Shared sine amplitude and pan receivers retain the remainder of the phase-word
quotient when reading the waveform. They interpolate adjacent compatible
phase-coordinate responses, including the wrap to the first table coordinate;
exact table coordinates keep their existing values. Fractional depth and phase
remain independent coordinates. The clock's 32-bit phase-word wrapping, sample
period, held-query spans and random history remain unchanged. Pitch, pulse-width,
FM and filter sine receivers retain compatible whole table coordinates where
phase interpolation exceeds the audio comparison bound. Native FM receivers keep
these query positions and histories; downstream product precision and compatibility
phase correction use a common path regardless of waveform or envelope stage.
That correction does not add coefficient filtering. Its audible phase correction
and separate compatible Sync reference are defined in
[audio rendering](audio-rendering.md).

## Input coordinates and consumption

Receiver admission is resolved per LFO slot in the render adapter. A prepared
shared receiver is consumed once; another unconsumed slot must retain its input.
Keep the original receiver view for physical control scheduling, even when a
separate audio-consumption view excludes a slot. Do not infer a different clock,
random seed or history from the coordinate conversion.

Prepared normalized signals that lack an admitted receiver are projected through
the compatible destination/depth endpoints. Pitch uses128 units per octave,
filter uses1024 units of headroom-relative depth, pulse width uses128 units of
relative width depth, and FM uses512 units of relative FM depth. Interpolate depth
knots and retain real signal coordinates. This defines the extension for those
unadmitted signals; it does not establish their original-DLL execution history.
Amplitude uses2048 units of per-slot attenuation and reaches the same physical
VCA for admitted and remaining inputs. Pan uses64 units per side and reaches
the output PanLfo receiver for admitted and remaining inputs alike.

Additional filter/FM controls enter the physical receiver before cutoff/depth
mapping and slewing. Retain every actual change boundary and the original native
receiver spans; a constant additional control does not force sample-sized calls.
Pitch controls compose before the common pitch curve, and pulse controls compose
with admitted LFO/envelope input before the common pulse-width curve. No linear
FM phase multiplier or octave-offset filter approximation is selected by history.
A missing prepared source is neutral, including unipolar filter/FM destinations.

MIDI wheel and routed pitch use ControllerRuntime destination setters. Keep running
inputs separate from mapped/source/getter/state values. There is no additional
Voice wheel-target/amount route. Ordinary pitch bend has channel-owned receivers;
native omni ingress uses channel1. Preserve its7-bit slew targets and signed
128-units-per-octave conversion separately from the14-bit routed pitch input.
Equal-value bend messages still request a root receiver update. Source/history
admission does not select an alternative tuning or bend transfer.

## Verification and limits

[ModulationBehaviorTest.cpp](../../tests/ModulationBehaviorTest.cpp) checks
waveform bounds, depth/phase, tempo response, key synchronization, disabled shared
clock advancement and read-only consumer access.
The tests do not prove all combinations
of edits, prepares, MIDI traffic or sample rates. [Shared modulation](../shared-modulation.md)
defines the component ownership and ordering boundary.

[HistoryQueueBehaviorTest.cpp](../../tests/HistoryQueueBehaviorTest.cpp) checks
queue saturation, wraparound/reuse, concurrent producers and bridge recovery of
publication after a drain. The realtime suite also checks that draining a saturated
history queue does not use the measured heap allocators.

Amplitude control composition retains shared held spans and every additional
signal change boundary. Preserve aligned gain storage, per-core VCA state and
release/retirement behavior. Sustain-stage VCA gain changes may carry an explicit
interpolation horizon across buffer fragments for projected continuous inputs or
generated ARP events. ARP gain spans follow a 64-frame grid anchored to the
Engine sample position. Known MIDI, ARP and receiver boundaries cap repeated
spans, including events that emit no note. This gain-only grid does not partition
or advance shared LFO/ARP clocks. Complete physical receiver dispatches use their actual
length; do not extend them with an unconditional64-frame horizon. Receiver-provided held spans bound
that horizon; a call without one declares a complete gain dispatch. Keep amplitude
envelope lifetime separate from gain interpolation. Release segments retain their
remaining natural horizon when a buffer ends inside an explicitly extended
dispatch; complete native dispatches retain their original progression.
Shared clocks retain their native setter/query/advance cadence; changing that
cadence requires separate compatibility validation.
There is no shared/nonshared amplitude renderer mode
or Voice-level postmix tremolo factor. The explicit offline envelope bypass
bypasses the entire VCA, including its LFO attenuation.

Pan projection is stored by Engine-owned source generation using the depth of
each actual MIDI subregion. Later output processing must not re-normalize earlier
samples with the final depth of a host block. Per slot, use admitted receiver
values or that prepared projection, never both. Do not promote source/history
admission flags merely because the output DSP can consume the signal.

All pan LFO inputs use the existing output cubic response, smoothing and
retained device state after MultiFX/Tone and before wet effects. Voice consumes
no global pan-LFO offset; its core/unison placement remains separate. Output
chunks must stay within the current prepared source block, including oversized
host callbacks. Preserve admitted output-region timing, semantic-silence behavior
and the receiver's existing stereo-only domain. Static non-LFO balance also
belongs to output, after MultiFX/Tone and before wet effects, independently of
source/history admission. Common output processing does not prove an unknown
source's native history.
