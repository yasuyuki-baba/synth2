# Voice audio component boundaries

| Component | Input boundary | Mutable state owner |
| --- | --- | --- |
| OscillatorAudioRenderer | Oscillator settings, frequencies, control words and read-only LFO view | Caller-owned OscillatorSection |
| PulseWidthRead | Real pulse-offset coordinate and caller-provided sample lookup | Stateless |
| FmPhaseCorrection | Applied FM depth/source, phase increment and native correction enable | Caller-owned OscillatorSection |
| FilterAudioRenderer | Filter mode/resonance/saturation, signal and resolved real cutoff index | Caller-owned Filter |
| AmplitudeBlockRenderer | Per-slot shared/additional attenuation, LFO spans and aligned gain region | Caller-owned AmplitudeEnvelope |

Components must not depend on Voice, Engine, patch documents, SDK parameters or
the full render aggregate. `render/VoiceAudioConfiguration.h` adapts aggregate
settings at the segment boundary. All use the portable audio/types layer.

Voice retains physical-core selection, noise draws, stereo summing and retirement.
Compose pitch controls before the single lookup, retaining fractional receiver
values and exact integer anchors; preserve FM/PW
selection, filter setup/bypass order, VCA gain partitions and buffer alignment.
No component extraction may introduce a new RNG source, allocation or reset.
See [audio rendering](contracts/audio-rendering.md). Public checks in
[DspBasicsTest.cpp](../tests/DspBasicsTest.cpp) and
[FilterBehaviorTest.cpp](../tests/FilterBehaviorTest.cpp) use generated signals
for oscillator pitch, envelope lifecycle and filter properties. They do not
establish every receiver history or original-engine sample identity.

OscillatorSection owns the common native FM phase correction and the compatible
local phase used by Sync/slave/noise resets. Keep that reference distinct from the
audible phase correction. Voice resets correction on an explicit oscillator phase
start; the render adapter selects native FM independently of LFO waveform or
modulation-envelope stage. The correction accepts the applied coefficient without
an extra filter; it needs no envelope signal or pending-transition span. Native
receiver partitioning remains in OscillatorControlRenderer, with caller-owned
real FM depth and existing control spans.

[FmPhaseCorrectionTest.cpp](../tests/FmPhaseCorrectionTest.cpp) checks compatible
phase bounds, unfiltered coefficient changes, reset/disabled behavior and
synchronized-noise history with generated inputs. Compatibility feedback bounds
fractional-product drift; it is not unbiased fractional phase accumulation.

FilterAudioRenderer consumes one cutoff coordinate prepared from the physical
filter envelope and receiver controls. It does not select a second envelope or
cutoff approximation. Oscillator envelope generation likewise belongs to the
physical core; the router composes remaining LFO/performance signals only.

Static and modulated FM consume the same physical-core depth buffer and phase
correction. Voice owns no separate static-FM smoother, and the audio adapter has
no static/modulated depth selector. History admission still governs generic
LFO/performance input transport without choosing another FM coefficient engine.

ModulationRouter converts only unconsumed prepared inputs to explicit pitch,
filter, pulse and FM receiver coordinates. The render adapter owns the per-slot
consumption mask. FM/filter extras enter control rendering before transfer/slew;
the audio component accepts resolved FM depth and an additional real pulse control.
There is no normalized FM offset or provisional phase multiplier in that adapter.

AmplitudeBlockRenderer accepts admitted and additional per-slot controls in the
same attenuation coordinates. Both independent factors reach the physical VCA;
keep each signal boundary and native output alignment. No postmix amplitude-LFO
gain or shared-renderer mode selects another amplitude path.

StereoBalance supplies only static left/right balance coefficients. Voice core
placement and output balance keep distinct positions and their defined float
multiplication order. PanLfo retains its cubic response,
state and two-millisecond half-life; common static coefficients do not move pan
across nonlinear effects or replace that receiver with a per-voice panner.

Pan LFO is an output responsibility for every input source. Voice retains core
and unison placement but has no global pan offset or unused per-voice pan state.
Engine-owned LfoControlProjection prepares real pan controls during each source
span; the output adapter selects one admitted/projected input per slot. The
output enable expresses an active pan receiver, not source admission. The cubic
curve, smoothing and lifecycle are the same physical PanLfo state for both.

Static global balance is also an output responsibility for every caller. Voice
retains per-core/unison positions only, without a separate global sum factor.
OutputPipelineSettings has no native-performance flag selecting static balance. Keep
source/history-aware scheduling separate from that kernel and retain the
mono/stereo coefficient rules.
