# Audio rendering contract

## Optional audio oversampling

The instance quality selector supports 1x (default), 2x, 4x, 8x and 16x. Keep
existing audio arithmetic and source/loading semantics unchanged at 1x.
Oversampling is global processing configuration, saved with processor/session
state and retained across patch loads; it is not a patch dial or source-version
policy. Old states without the setting restore 1x. Unsupported factors are rejected
transactionally. Append the native non-automatable quality parameter after the
existing 129 host controls without renumbering them.

At higher factors, preserve oscillator phase, FM feedback, Sync reset, stack/sub
and oscillator-noise updates at the host rate. Reconstruct intermediate waveform
reads along the signed phase advance of each base frame, retaining Sync resets
at its boundary. Do not infer the advance from the shortest circular phase arc,
scale the FM depth by the factor, or reset FM history on quality changes.
Oscillator noise holds the base-frame sample without square-root gain scaling.
This retains the discrete feedback model; it does not remove aliasing already
embedded in that model or claim a continuous-time FM/Sync replacement.

Run the complete per-core filter, limiter and saturation at the internal rate.
Apply base-rate VCA and pan values to fine samples, sum them, then decimate
globally so tails outlive logical voice retirement. Keep the fixed 22 kHz base
frequency ceiling. Shared noise/RNG and MIDI, ARP, LFO, portamento and all
FM/cutoff/envelope receivers retain the host cadence. Hold resolved controls
across each internal audio frame group.

The output drive uses interpolation/decimation at the selected factor. Run
its overdrive kernels and tone filters at that rate. Other output processing,
including intentional decimation, Ring, compressor, EQ, pan, Delay and Chorus,
keeps its host-rate cadence. Keep a consistent interpolation/decimation stage
before those effects even when drive is off, so latency is independent of patch.

Use prepared 65-tap linear-phase half-band stages and phase-zero decimation.
Voice decimation plus the output interpolation/decimation pair produces fixed
host-frame delays of 0/48/72/84/90 at 1x/2x/4x/8x/16x. Expose the delay to native
hosts. A live selection fades out before applying the new internal rates and
fades back in; do not allocate or reset MIDI/control history during the switch.
Higher-factor rendering deliberately changes timbre and latency and is not a way
to force original-engine error bounds. Compatibility comparisons retain 1x.

## Required behavior

Preserve the supported control meanings, integer/float operation order, frequency
rounding, fractional sample rates, receiver partitions, buffer alignment, RNG
advancement and state lifetimes. Treat changes to these as behavioral changes.
Choose processing from controls/history, never preset titles, numbers or hashes.

Normalize pulse-width dials with a float reciprocal multiply before modulation.
Preserve the compatible integer source-dial/table-bin knots and their operation
order; division can round differently at a bin boundary. PWM interpolates those
knots in both the base dial and receiver coordinate before audio lookup. Static
width edits use the same mapping as modulated width edits, preserving fractional
base values instead of quantizing them back to a whole table bin. Ordinary and
stack oscillators share PulseWidthRead interpolation for every offset; retain
their existing phase lookup, gain normalization and lane summation rules.
Repeated identical base-width/modulation coordinates may reuse their converted
pulse offset; a change to either coordinate must refresh it without changing
phase, receiver clocks or RNG state. Audio stays Float32. At 1x, do not initialize
unused fine-frame accumulation buffers; higher factors clear their active lanes
before summing the same per-core signals in the existing order.

Preserve all-core envelope → glide → FM → filter control ordering. Within a sample,
preserve oscillator → noise RNG → filter → VCA/pan ordering. Keep OSC2 coarse
exponent, cached frequency and pending-note resets distinct, including legato FM
partitioning/refresh behavior. Logical voice state cannot replace physical receiver
activity or justify skipping retained state.

All callers use one shifted-note and tracked/fixed OSC2 tuning calculation.
OSC2 coarse uses its physical receiver for all callers, retaining cached/pending
state and FM partitions. Glide comes from physical-core Portamento with the
resolved source dial, including fractional edits. Do not restore a separate
Voice exponential frequency glide or tuning-selection flags. MIDI note and
semitone group selection remain discrete; fine controls retain fractional cents.

Both oscillator frequencies have a fixed 22,000 Hz ceiling after pitch
modulation, before table selection and phase-increment calculation. Apply it on
ordinary scalar, FM, Ring and Sync paths as well as stacked/modulated paths. It
is independent of sample rate and source format; it is not a sample-rate/2
formula. Preserve frequencies below the ceiling and its exact endpoint. A
negative pitch offset must act before limiting, so a 40 kHz base shifted down
one octave produces 20 kHz.

Output processing applies nonlinear MultiFX/Phaser and Tone/effect processing,
then global balance or Pan LFO, then native Delay and Chorus, then startup gain.
Wet processing uses EffectPair for every source/state route when effects are
enabled and the sample rate is from 10,000 through 384,000 Hz inclusive. Outside
that domain, wet processing bypasses without selecting an alternative algorithm. Silent input
does not imply an empty effect tail or allow skipping gain advancement. Preserve
semantic silence, recurrence boundaries and the output-device prepare/release
retention rules. Shared LFO views remain read-only throughout output processing.

Do not allocate, block on locks or perform I/O in audio callbacks. Float WAV
capture must preserve headroom above unity. Raw stereo comparison does not align,
downmix, normalize or fit gain; those transformations may be separate diagnostics
but cannot replace compatibility evidence.

## Common table reads and amplitude

All ordinary oscillator table reads interpolate adjacent samples. Source formats,
FM routing and modulation admission do not select an eight-bin average reader.
Pulse, stack and nonlinear mode kernels retain their own defined phase/gain rules.

Mapped amplitude uses the same gain curve and VCA gain application for ordinary
notes and arpeggios. Source format does not select an alternate gain calculation.
Deterministic amplitude LFOs share the clock, receiver knots and VCA target/ramp
processing across formats. Each slot is consumed once at its destination; no
normalized modulation router or duplicate postmix factor remains.
Keep fractional phase interpolation and the existing real envelope levels.
VCA Attack targets retain fractional levels rather than truncating to integer
steps. Distortion-sensitive reference errors must not select an integer Attack
path or a different amplitude algorithm.

## Filter implementation selection

A selected compatible Filter kernel processes its entire resonance range with
one retained state. Crossing resonance dial112 must not switch to a different
filter model or discard its running history. Cutoff/envelope coordinate adapters
and direct-index callers share that kernel and the existing post-filter gain and
saturation order.

Every source route uses that filter family and canonical output tone/peak
processing. There is no alternate approximation or unison gain correction.
Control scheduling retains setter/query/advance histories after source conversion.
Evidence validity does not select another audio or receiver implementation. At low
sample rates, cutoff is bounded by Nyquist and recurrence coefficients stay in
their stability domain. Common render policy resolves controls and explicit
lifecycle decisions without source/history admission branches.
Resolve sub-receiver
presence from the incoming real level before compatible level calibration; a
positive fractional sub dial below0.5 must retain the native gain scale both
with and without oscillator stacking.

## Fractional output controls

Synth2 continuous-control states and version 200 patches retain fractional EQ,
Delay and Chorus dials through the output adapters. Legacy integer-state mapping
remains unchanged: legacy source/state adapters resolve integer output controls
before publishing real values to DSP. Output settings carry no precision-mode
flag; direct DSP callers also retain fractional dials. Integer dial positions
retain compatible coefficient knots, neutral/bypass behavior and device history.
Recover exact integer words from their native normalized float representations
before applying fractional interpolation; do not treat normalization residuals
as intentional edits.

All EQ callers share PeakEq and OutputTone; neutral controls, explicit bypass and
semantic silence remain distinct. Peak EQ extends its frequency/gain/Q mapping to fractional dials. Tone filters
interpolate adjacent native one-pole coefficients, including neutral bypass knots,
and use the same three-coefficient sample recurrence for every dial position.
Floating operation reassociation at integral settings remains subject to the audio
error policy; it is not assumed to preserve PCM bytes.
Delay/Chorus continuous controls interpolate native control knots or extend their
linear gains. Chorus depth retains sub-sample modulation extent between integer
knots. Type/voice selections remain discrete. Tempo-delay time interpolates the
existing timing knots with Float32 BPM coordinates. Resolve aligned/clamped
native frame anchors before interpolating fractional time/spread controls and
processing taps, preserving the native four-frame alignment at those anchors.
This does not introduce a new sync mode or beat table.
Delay time/spread interpolate the compatible frame targets after native alignment
and clamping. Fractional targets retain real sample coordinates and use causal
linear taps. Chorus base delay and period interpolate compatible sample-count
anchors. All Chorus time/depth/rate settings use common real tap coordinates and
modulation extent, including mirrored taps. Integer control knots retain their
compatible modulation amplitudes; sample processing does not quantize tap positions
to 16 fractional bits. Integer settings retain the native oscillator recurrence,
phase update and retirement timing. The Chorus recursive modulation clock retains
its FP32 phase/coefficient words: its block reset comparison depends on those words.
Audio delay-line samples and feedback audio histories remain Float32; moving
delay coordinates retain double precision.
Keep Chorus ring indices separate from fractional tap offsets so the integer
cursor and ring wrapping do not reduce sub-sample interpolation precision.
Ring frequency/phase-step knots and Phaser sweep-factor knots retain compatible
word definitions before fractional interpolation. Phaser direction changes select
the opposite compatible factor rather than inverting a widened factor.
Buffer indices and sampled event positions remain discrete.

Changing fractional controls intentionally changes audio. Native integer-point
preservation and useful fractional resolution are separate checks. Do not infer
that higher precision automatically sounds better or matches a native DLL at
fractional settings it cannot represent. MultiFX and Phaser retain fractional
controls; intentional decimator bit depth remains discrete, while its fractional
sample period keeps the sample-clock remainder. Amplitude gain/velocity sensitivity
and saturation interpolate compatible integer anchors. Fine tuning retains
fractional cents outside its native neutral band. Native LFO/FM/envelope control
stepping remains where removing it violates the audio comparison bound.

Receiver signal buffers and FM/cutoff running values use Float32;
segment lengths and sample counters remain integers. This distinction preserves
fractional modulation as well as exact whole 27-bit FM phase units. Pitch, pulse
width, FM, filter and amplitude consumers accept fractional receiver values.
Compatible transfer/slew curves retain their integer-coordinate response and
interpolate between those knots without narrowing the running signal. Filter
coefficient caches include the fractional cutoff coordinate. These changes do not
remove integer LFO clock counters/sample periods, compatible portamento
pitch-curve knots or the oscillator phase accumulator. Sine amplitude and pan
waveform reads retain fractional phase-table coordinates; other sine receivers
keep their compatible table positions.

The native filter base extends the integer-dial cutoff curve for fractional
edits. Its render adjustment is separate from the fallback cutoff coordinate;
integer controller setters replace that adjustment. Keyboard offsets retain
fractions through physical core state and receiver transport. Envelope headroom,
peak and sustain extend compatible base/tracking/depth knots without narrowing
the real coordinates. Integer source dials retain their original response.

Unison Detune retains fractional dials in voice tuning and filter layout. Pitch
values and physical-core filter offsets extend compatible integer knots without
narrowing their transport. Filter headroom, peak and sustain include fractional
unison offsets. Voice count and semitone group transposition remain discrete.
Fixed-phase unison Spread retains fractional dials until phase-accumulator
conversion and uses the same deterministic local table, without shared draws.
The fixed oscillator phase amount retains fractions for OSC2 and stack lanes.
Keep its base/spread coordinates in Float32 between compatible integer-dial phase
words, then convert the interpolated phase coordinate to the accumulator word.
Draw each local random value once before interpolation; evaluating neighboring
control knots must not consume extra draws.
Random/fixed selection retains its rounded native boundary; edits inside random
mode do not request new draws. Stack Detune retains fractions through settings,
spacing, lane-frequency calculation and cache keys. Rounded-zero stack selection
remains scalar. Integer phase accumulators remain a separate boundary.

Deterministic shared LFO depth keeps fractional sine/triangle/saw/square receiver
values. Portamento time retains fractional source dials through compatible time
knots and the native duration adapter. Actual duration counters and segment
boundaries remain integer samples. Integer dial settings retain their response.
Portamento offset/delta state and receiver output retain fractions across
retriggers, while MIDI endpoint mapping and the compatible whole-state curve
remain stepped. Random LFO held/smoothed state and effective depth ranges also
retain fractions without changing the shared CRT recurrence or integer-setting
draw order. See [modulation](modulation.md) for history and draw boundaries.

Amplitude, filter/modulation and release envelopes store real levels, peak and
sustain values, and retain them through release handoff and receiver callbacks.
Fractional amplitude sustain, filter depth/sustain and modulation-envelope depth
reach those states through the loading/render adapters. Integer level settings
retain the compatible response knots and segment schedule. Live depth edits
rescale the visible real level immediately, including peaks below one level.
Use the same real-valued rescaling for integral and fractional peaks/levels;
integer proximity must not select a different formula. During Release, retain
the real starting level captured at note-off: a peak edit rescales the visible
level without replacing the remaining release trajectory. Host edits to filter
sustain, cutoff, envelope depth or keyboard tracking restart the cutoff receiver's
50ms slew from its current value, including on releasing physical cores. Consecutive
identical parameter/value setters are neutral; fractional value changes remain
effective even within one integer dial. Ordered filter edits retain intermediate
values and configure sounding filter envelopes before the callback's MIDI events; zero-depth round trips must not be
collapsed into their final value. Cutoff clocks may restart without advancing
audio. A bounded publication queue reports lost edits and converges to the final
mapped controls if saturated; unrestricted history beyond its capacity is not
promised. State replacement discards pending edits for the preceding sound. Zero peaks clear the visible level, and
restoring a peak from zero clears the corresponding real release state. Filter peak
and sustain configuration shares one compatible-knot evaluator; exact source
normalization is retained at integral sustain knots.
A full-cutoff round trip that returns before any audio is rendered must preserve
an existing nonzero real envelope position, including a temporary level below
one control unit. Do not emulate legacy integer rescaling that rounds that level
to zero and loses the envelope state. This is an intentional behavior difference:
the subsequent sound may differ substantially from a reference that discarded
that state. An explicit zero-depth edit still clears its real state as specified
above; this decision does not permit dropping intermediate edits. Report the
behavior decision separately from numerical original-audio acceptance; see
[validation](validation.md#intentional-behavior-differences).

Durations, counters and callback partitions remain discrete sample metadata;
modulation-envelope Attack/Decay dials retain fractions through compatible time
knots before the duration is converted to sample counts.

## Verification and limits

The public [DSP basics](../../tests/DspBasicsTest.cpp) tests check generated-sine
pitch and production envelope lifecycles.
[Effects behavior](../../tests/EffectsBehaviorTest.cpp) and
[generated inputs](../../tests/GeneratedInputTest.cpp) check cold reset/bypass,
finite output and buffer/state isolation under bounded inputs.
The public [filter](../../tests/FilterBehaviorTest.cpp) and
[equalizer](../../tests/EqualizerBehaviorTest.cpp) tests exercise generated-signal
properties, reset behavior and channel independence. The public host/state tests
exercise lifecycle and input contracts without recorded original outputs.

Exact scalar/control assertions and bounded composed-audio assertions provide
different evidence. Sampled success is not complete bit identity or unrestricted
audio equivalence. See [validation](validation.md).

## Bounded precision improvements

Audio buses and most signal histories use Float32. Diode filter states and
sample arithmetic, moving delay positions and coefficient-generation
intermediates retain double precision where their numerical requirements demand
it. Preserve the precision of the original calculation rather than imposing one
scalar type throughout waveform reads, filters, VCA/pan and output effects. The bridge uses Float32 scratch storage and
applies transition gains before copying to float host outputs or widening to
double host outputs. Both host formats execute the same DSP path. UI geometry,
host parameter APIs and serialized state/diagnostic formats remain separate
boundaries; they cannot select another processing precision.

Use the arithmetic precision required by each kernel. LP24/SVF
coefficient preparation preserves double intermediates for frequency mapping,
LP24 pole/feedback and smoothing expressions, and SVF sine/gain preparation.
Round the resulting ordinary-filter coefficients to Float32 before their sample
recurrences. Tone/EQ and Overdrive coefficient calculations also preserve their
double intermediates. The diode kernel retains double states and sample
arithmetic before rounding its output to the Float32 audio bus. This applies to integral and fractional controls without a precision
selector or a second renderer. Prepared waveform
constants keep their calibrated Float32 definitions; offline generation of those
constants preserves their storage precision. Prepared time/rate/spread
words preserve integer event/tap boundaries. Tuning uses Float32 table words and
FMA compensation to avoid prematurely rounding the exponential ratio before its
frequency product; control inputs and returned frequencies remain Float32. FM source/control integer
knots resolve through calibrated integer phase words on the exact 1/512 source
grid before Float32 control interpolation. Audio and FM/Sync use the same
Float32 oscillator read. Control interpolation retains representable fractions
between compatible integer-dial knots; edits finer than Float32 resolution round
at the input boundary. There is no sub-float input interpolation or promise that
every real-valued edit changes a sample.

Frequency increments remain cached by effective Float32 frequency and invalidated
by sample-rate changes. Integer phase accumulators and frame/span counters retain
their discrete representation. Convert durations to Float32 sample products before
final integer frame conversion. Keep floating envelope levels; Float32 processing
must not restore whole-level truncation during Attack. Tracked OSC2 ratios, native
control knots and the distinction between moving and prepared target frequencies
retain their control meanings and operation order.

Within one voice render span, unchanged coarse receiver exponents may reuse the
same mapped frequency for each physical core. Base/fine inputs are fixed within
that span; initialize the result on its first use and discard it at the span
boundary. Reuse must not advance or reset receivers, phase, envelopes or RNG.

Legacy loading resolves the compatible integer parameters before promoting those
exact values to floating-point dials. Fractional edits follow promotion; source
conversion must not run a second time. DSP-derived initial sample counts and
levels are outside this loading-state boundary.

Higher numeric precision must retain the response and update timing at native
integer settings. Extend fractional settings between compatible anchors. Exact
waveform identity is not required. Raw complete-stereo normalized error below
0.005 passes normally. Errors from 0.005 to strictly below 0.01 require a recorded
cause review confirming an intentional precision difference and preserved control,
loading and update semantics. Unexplained differences in that band remain
unresolved; errors at or above 0.01 fail. Report conditional acceptance separately
from reaching the 0.005 target; see [validation](validation.md). Preserve discrete
protocol, selection and event/index boundaries where they define native behavior.

The VCA Attack target is the real sample-progress ratio times 2048, including
whole source dials. Its audio gain ramp retains fractional target levels. On
retrigger, derive the discrete Attack sample position from the whole-level
anchor (truncate level before multiplying by duration), while retaining the real
level and current gain. Fractional audio levels must not feed back into this
compatible restart-position conversion. Decay
and Release retain compatible response knots and timing rules. Native filter
kernels receive real cutoff coordinates through both the control receiver and
the sample-processing wrapper. The calibrated high-resonance fallback retains
its integer cutoff table positions.

Voice amplitude uses the physical-core VCA for all callers, with one absolute
gain/assigned-velocity mapping and aligned per-core gain processing. Natural VCA
completion owns amplitude retirement; no second sample envelope or release clock
selects a different path. Filter and oscillator modulation also use their
physical-core envelopes for every caller. Filter control rendering composes the
real envelope, tracking and cutoff state before the audio kernel; there is no
parallel sample-clocked filter envelope. Oscillator pitch, FM and pulse-width
destinations use the resolved Attack/Decay/depth dials and one physical AD state,
including unverified LFO histories. The unused direct-call both-pitch envelope
destination is removed; mapped oscillator destinations remain OSC2 pitch, FM and
pulse width. LFO slots enter the physical receiver once without an admission selector.
The compatible VCA Decay/Release curves remain unchanged.
Static and modulated FM use one physical-core FrequencyModulation state and
receiver renderer. Base-dial setters retain the sample-rate-derived slew horizon,
32-frame coefficient knots and real current/anchor/target values. Cold/idle
starts settle; sounding retriggers invalidate target/elapsed while retaining a
live slew. Legato retains its existing assignment rules. There is no Voice-owned
fixed512-sample smoother or audio input selector between static/modulated depth.
Static FM mapping interpolates compatible integer control knots after legacy
loading; fractional edits do not round back to whole dials. Oscillator phase-word
conversion remains a separate limit.

Native FM uses a common downstream phase correction, independent of LFO waveform,
waveform mixtures or modulation-envelope enable/stage. Waveform generation and
native held-query spans remain waveform-specific. Preserve modulation composition,
FM depth mapping and receiver timing; the phase correction adds no coefficient
filter. Integer coefficient steps, turns and intentional jumps remain intact when
their products are integral. The physical FM receiver owns coefficient slewing independently of the
downstream phase correction.

OscillatorSection reads one Float32 audio sample for audio and FM/Sync from the
same phase and table entries. Prepared tables retain their calibrated Float32
words. FmPhaseCorrection receives Float32 depth and source values. It interpolates
differences from the compatible integer phase-product word, so large integer
phase offsets are never added to fractional corrections in floating point.
The reference offset remains `int32(depth * source)`. A Float32 residual and an
integer whole offset compose the corrected signed phase word with truncation
toward zero. Accumulated
product error feeds back toward that compatible trajectory over the native
32-frame coefficient interval; a fractional carry retains the remainder when
converting the corrected audible offset to a phase word. This feedback deliberately
bounds drift relative to compatible integer FM. It is a compatibility policy,
not unbiased fractional integration or a claim that every precision boundary has
been removed. Constant-input quantization must not cause unbounded phase drift,
and accumulated correction must return after the fractional product error settles.

OscillatorSection owns this state and a separate local compatible reference phase
for Sync/slave/noise resets. Advance that reference online with the live compatible
FM product, separately from the corrected audible phase, without captured signals
or another engine. An LFO/envelope coefficient change must not clear phase history.
Explicit oscillator phase starts and external phase rebases reset the correction.
The render adapter enables common correction for every instrument FM route;
explicit offline envelope bypass and Ring dispatch retain the compatible
uncorrected path. Finite-window differences
remain subject to the raw stereo audio bound. These numerical properties do not
establish perceptual improvement or unrestricted equivalence.

## Ring and sub composition

Ring uses the same product-mix expression for every caller, before FM dispatch.
Selecting Ring suppresses FM phase offsets. Pulse/noise wrapper gains can still
change when FM is selected; these are device semantics rather than alternate
Ring algorithms. Preserve the multiplication/addition order in the Float32 signal path.

Sub uses one level mapping (four times the normalized source dial) and the
compatible carrier/sub/mix normalizer for every render route. Its full OSC1-linked
phase advances even while the sub level is zero, retaining the extra octave bit
for the lower octave. Stacked and ordinary reads keep their established lookup,
interpolation and lane summation order. Logical note changes must not invent an
independent sub phase or erase retained physical history.

All LFO slots use one prepared physical-control block and shared clock/CRT state.
Evidence validity is diagnostic, not an audio-implementation selector. Preserve
native query/advance cadence and numerical knots across these consumers.
See [modulation input coordinates](modulation.md#input-coordinates-and-consumption).

All amplitude LFO inputs reach independent per-slot physical VCA attenuation,
with shared held spans retained. No Voice-level
postmix tremolo factor remains. Explicit offline envelope bypass bypasses the
whole VCA and its modulation. StereoBalance shares static balance coefficients
across core, unison, output and PanLfo callers while preserving their processing
stages and multiplication order. PanLfo's cubic curve/slew remain separate from
static balance; moving gain across nonlinear processing is a behavior change.

Pan-LFO sources compose at output in real64-unit coordinates, after MultiFX/Tone
and before wet processing. Engine prepares physical pan controls at their
MIDI subregions; output consumes each active slot once. Global pan
LFO is not applied per Voice. Retain core/unison positions, native output-region
boundaries, pan device history and semantic-silence handling. The receiver keeps
its existing stereo-only behavior; static non-LFO balance uses the same output placement for every source.

Static global balance always follows MultiFX/Tone and precedes wet effects.
It is not multiplied into a Voice or a summed unison group. Voice applies only
per-core/unison positions; OutputPipelineSettings carries no history-based static-balance
enable. Preserve native region timing and multiplication order. Mono static
balance retains the average of the two compatible static coefficients; this does
not extend the separate stereo-only PanLfo receiver.
